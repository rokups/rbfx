//
// Copyright (c) 2008-2020 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "../Precompiled.h"

#include "../Core/Context.h"
#include "../Core/Profiler.h"
#include "../IO/File.h"
#include "../IO/FileSystem.h"
#include "../IO/Log.h"
#include "../IO/MemoryBuffer.h"
#include "../IO/PackageFile.h"
#include "../Network/Connection.h"
#include "../Network/Network.h"
#include "../Network/NetworkEvents.h"
#include "../Network/NetworkPriority.h"
#include "../Network/Protocol.h"
#include "../Resource/ResourceCache.h"
#include "../Scene/Scene.h"
#include "../Scene/SceneEvents.h"
#include "../Scene/SmoothedTransform.h"

#include <slikenet/MessageIdentifiers.h>
#include <slikenet/peerinterface.h>
#include <slikenet/statistics.h>

#ifdef SendMessage
#undef SendMessage
#endif

#include "../DebugNew.h"
#include "Connection.h"


#include <cstdio>

namespace Urho3D
{

static const int STATS_INTERVAL_MSEC = 2000;

PackageDownload::PackageDownload() :
    totalFragments_(0),
    checksum_(0),
    initiated_(false)
{
}

PackageUpload::PackageUpload() :
    fragment_(0),
    totalFragments_(0)
{
}

Connection::Connection(Context* context) :
    Object(context),
    timeStamp_(0),
    peer_(nullptr),
    sendMode_(OPSM_NONE),
    isClient_(false),
    connectPending_(false),
    sceneLoaded_(false),
    logStatistics_(false),
    address_(nullptr),
    packedMessageLimit_(1024)
{
}

Connection::~Connection()
{
    // Reset scene (remove possible owner references), as this connection is about to be destroyed
    SetScene(nullptr);

    delete address_;
    address_ = nullptr;
}

void Connection::Initialize(bool isClient, const SLNet::AddressOrGUID& address, SLNet::RakPeerInterface* peer)
{
    assert(peer_ == nullptr);
    peer_ = peer;
    isClient_ = isClient;
    sceneState_.connection_ = this;
    port_ = address.systemAddress.GetPort();
    SetAddressOrGUID(address);
}

void Connection::RegisterObject(Context* context)
{
    context->RegisterFactory<Connection>();
}

PacketType Connection::GetPacketType(bool reliable, bool inOrder)
{
    if (reliable && inOrder)
        return PT_RELIABLE_ORDERED;

    if (reliable && !inOrder)
        return PT_RELIABLE_UNORDERED;

    if (!reliable && inOrder)
        return PT_UNRELIABLE_ORDERED;

    return PT_UNRELIABLE_UNORDERED;
}

void Connection::SendMessage(int msgID, bool reliable, bool inOrder, const VectorBuffer& msg, unsigned contentID)
{
    SendMessage(msgID, reliable, inOrder, msg.GetData(), msg.GetSize(), contentID);
}

void Connection::SendMessage(int msgID, bool reliable, bool inOrder, const unsigned char* data, unsigned numBytes,
    unsigned contentID)
{
    if (numBytes && !data)
    {
        URHO3D_LOGERROR("Null pointer supplied for network message data");
        return;
    }

    PacketType type = GetPacketType(reliable, inOrder);
    VectorBuffer& buffer = outgoingBuffer_[type];

    if (buffer.GetSize() + numBytes >= packedMessageLimit_)
        SendBuffer(type);

    if (buffer.GetSize() == 0)
    {
        buffer.WriteUByte((unsigned char)DefaultMessageIDTypes::ID_USER_PACKET_ENUM);
        buffer.WriteUInt((unsigned int)MSG_PACKED_MESSAGE);
    }

    buffer.WriteUInt((unsigned int) msgID);
    buffer.WriteUInt(numBytes);
    buffer.Write(data, numBytes);
}

void Connection::SendRemoteEvent(StringHash eventType, bool inOrder, const VariantMap& eventData)
{
    RemoteEvent queuedEvent;
    queuedEvent.senderID_ = 0;
    queuedEvent.eventType_ = eventType;
    queuedEvent.eventData_ = eventData;
    queuedEvent.inOrder_ = inOrder;
    remoteEvents_.push_back(queuedEvent);
}

void Connection::SendRemoteEvent(Node* node, StringHash eventType, bool inOrder, const VariantMap& eventData)
{
    if (!node)
    {
        URHO3D_LOGERROR("Null sender node for remote node event");
        return;
    }
    if (node->GetScene() != scene_)
    {
        URHO3D_LOGERROR("Sender node is not in the connection's scene, can not send remote node event");
        return;
    }
    if (!node->IsReplicated())
    {
        URHO3D_LOGERROR("Sender node has a local ID, can not send remote node event");
        return;
    }

    RemoteEvent queuedEvent;
    queuedEvent.senderID_ = node->GetID();
    queuedEvent.eventType_ = eventType;
    queuedEvent.eventData_ = eventData;
    queuedEvent.inOrder_ = inOrder;
    remoteEvents_.push_back(queuedEvent);
}

void Connection::SetScene(Scene* newScene)
{
    if (scene_)
    {
        // Remove replication states and owner references from the previous scene
        scene_->CleanupConnection(this);
    }

    scene_ = newScene;
    sceneLoaded_ = false;
    UnsubscribeFromEvent(E_ASYNCLOADFINISHED);

    if (!scene_)
        return;

    if (isClient_)
    {
        sceneState_.Clear();

        // When scene is assigned on the server, instruct the client to load it. This may require downloading packages
        const ea::vector<SharedPtr<PackageFile> >& packages = scene_->GetRequiredPackageFiles();
        unsigned numPackages = packages.size();
        msg_.Clear();
        msg_.WriteString(scene_->GetFileName());
        msg_.WriteVLE(numPackages);
        for (unsigned i = 0; i < numPackages; ++i)
        {
            PackageFile* package = packages[i];
            msg_.WriteString(GetFileNameAndExtension(package->GetName()));
            msg_.WriteUInt(package->GetTotalSize());
            msg_.WriteUInt(package->GetChecksum());
        }
        SendMessage(MSG_LOADSCENE, true, true, msg_);
    }
    else
    {
        // Make sure there is no existing async loading
        scene_->StopAsyncLoading();
        SubscribeToEvent(scene_, E_ASYNCLOADFINISHED, URHO3D_HANDLER(Connection, HandleAsyncLoadFinished));
    }
}

void Connection::SetIdentity(const VariantMap& identity)
{
    identity_ = identity;
}

void Connection::SetControls(const Controls& newControls)
{
    controls_ = newControls;
}

void Connection::SetPosition(const Vector3& position)
{
    position_ = position;
    if (sendMode_ == OPSM_NONE)
        sendMode_ = OPSM_POSITION;
}

void Connection::SetRotation(const Quaternion& rotation)
{
    rotation_ = rotation;
    if (sendMode_ != OPSM_POSITION_ROTATION)
        sendMode_ = OPSM_POSITION_ROTATION;
}

void Connection::SetConnectPending(bool connectPending)
{
    connectPending_ = connectPending;
}

void Connection::SetLogStatistics(bool enable)
{
    logStatistics_ = enable;
}

void Connection::Disconnect(int waitMSec)
{
    peer_->CloseConnection(*address_, true);
}

void Connection::SendServerUpdate()
{
    if (!scene_ || !sceneLoaded_)
        return;

    // Always check the root node (scene) first so that the scene-wide components get sent first,
    // and all other replicated nodes get added to the dirty set for sending the initial state
    unsigned sceneID = scene_->GetID();
    nodesToProcess_.insert(sceneID);
    ProcessNode(sceneID);

    // Then go through all dirtied nodes
    nodesToProcess_.insert(sceneState_.dirtyNodes_.begin(), sceneState_.dirtyNodes_.end());
    nodesToProcess_.erase(sceneID); // Do not process the root node twice

    while (nodesToProcess_.size())
    {
        unsigned nodeID = *nodesToProcess_.begin();
        ProcessNode(nodeID);
    }
}

void Connection::SendClientUpdate()
{
    if (!scene_ || !sceneLoaded_)
        return;

    msg_.Clear();
    msg_.WriteUInt(controls_.buttons_);
    msg_.WriteFloat(controls_.yaw_);
    msg_.WriteFloat(controls_.pitch_);
    msg_.WriteVariantMap(controls_.extraData_);
    msg_.WriteUByte(timeStamp_);
    if (sendMode_ >= OPSM_POSITION)
        msg_.WriteVector3(position_);
    if (sendMode_ >= OPSM_POSITION_ROTATION)
        msg_.WritePackedQuaternion(rotation_);
    SendMessage(MSG_CONTROLS, false, false, msg_, CONTROLS_CONTENT_ID);

    ++timeStamp_;
}

void Connection::SendRemoteEvents()
{
#ifdef URHO3D_LOGGING
    if (logStatistics_ && statsTimer_.GetMSec(false) > STATS_INTERVAL_MSEC)
    {
        statsTimer_.Reset();
        char statsBuffer[256];
        sprintf(statsBuffer, "RTT %.3f ms Pkt in %i Pkt out %i Data in %.3f KB/s Data out %.3f KB/s, Last heard %u", GetRoundTripTime(),
            GetPacketsInPerSec(),
            GetPacketsOutPerSec(),
            GetBytesInPerSec(),
            GetBytesOutPerSec(),
            GetLastHeardTime());
        URHO3D_LOGINFO(statsBuffer);
    }
#endif

    if (packetCounterTimer_.GetMSec(false) > 1000)
    {
        packetCounterTimer_.Reset();
        packetCounter_ = tempPacketCounter_;
        tempPacketCounter_ = IntVector2::ZERO;
    }

    if (remoteEvents_.empty())
        return;

    URHO3D_PROFILE("SendRemoteEvents");

    for (auto i = remoteEvents_.begin(); i != remoteEvents_.end(); ++i)
    {
        msg_.Clear();
        if (!i->senderID_)
        {
            msg_.WriteStringHash(i->eventType_);
            msg_.WriteVariantMap(i->eventData_);
            SendMessage(MSG_REMOTEEVENT, true, i->inOrder_, msg_);
        }
        else
        {
            msg_.WriteNetID(i->senderID_);
            msg_.WriteStringHash(i->eventType_);
            msg_.WriteVariantMap(i->eventData_);
            SendMessage(MSG_REMOTENODEEVENT, true, i->inOrder_, msg_);
        }
    }

    remoteEvents_.clear();
}

void Connection::SendPackages()
{
    while (!uploads_.empty())
    {
        unsigned char buffer[PACKAGE_FRAGMENT_SIZE];

        for (auto i = uploads_.begin(); i != uploads_.end();)
        {
            auto current = i++;
            PackageUpload& upload = current->second;
            auto fragmentSize =
                (unsigned)Min((int)(upload.file_->GetSize() - upload.file_->GetPosition()), (int)PACKAGE_FRAGMENT_SIZE);
            upload.file_->Read(buffer, fragmentSize);

            msg_.Clear();
            msg_.WriteStringHash(current->first);
            msg_.WriteUInt(upload.fragment_++);
            msg_.Write(buffer, fragmentSize);
            SendMessage(MSG_PACKAGEDATA, true, false, msg_);

            // Check if upload finished
            if (upload.fragment_ == upload.totalFragments_)
                uploads_.erase(current);
        }
    }
}

void Connection::SendBuffer(PacketType type)
{
    VectorBuffer& buffer = outgoingBuffer_[type];
    if (buffer.GetSize() == 0)
        return;

    PacketReliability reliability = PacketReliability::UNRELIABLE;
    if (type == PT_UNRELIABLE_ORDERED)
        reliability = PacketReliability::UNRELIABLE_SEQUENCED;

    if (type == PT_RELIABLE_ORDERED)
        reliability = PacketReliability::RELIABLE_ORDERED;

    if (type == PT_RELIABLE_UNORDERED)
        reliability = PacketReliability::RELIABLE;

    if (peer_) {
        peer_->Send((const char *) buffer.GetData(), (int) buffer.GetSize(), HIGH_PRIORITY, reliability, (char) 0,
                    *address_, false);
        tempPacketCounter_.y_++;
    }

    buffer.Clear();
}

void Connection::SendAllBuffers()
{
    SendBuffer(PT_RELIABLE_ORDERED);
    SendBuffer(PT_RELIABLE_UNORDERED);
    SendBuffer(PT_UNRELIABLE_ORDERED);
    SendBuffer(PT_UNRELIABLE_UNORDERED);
}

void Connection::ProcessPendingLatestData()
{
    if (!scene_ || !sceneLoaded_)
        return;

    // Iterate through pending node data and see if we can find the nodes now
    for (auto i = nodeLatestData_.begin(); i != nodeLatestData_.end();)
    {
        auto current = i++;
        Node* node = scene_->GetNode(current->first);
        if (node)
        {
            MemoryBuffer msg(current->second);
            msg.ReadNetID(); // Skip the node ID
            node->ReadLatestDataUpdate(msg);
            // ApplyAttributes() is deliberately skipped, as Node has no attributes that require late applying.
            // Furthermore it would propagate to components and child nodes, which is not desired in this case
            nodeLatestData_.erase(current);
        }
    }

    // Iterate through pending component data and see if we can find the components now
    for (auto i = componentLatestData_.begin(); i != componentLatestData_.end();)
    {
        auto current = i++;
        Component* component = scene_->GetComponent(current->first);
        if (component)
        {
            MemoryBuffer msg(current->second);
            msg.ReadNetID(); // Skip the component ID
            if (component->ReadLatestDataUpdate(msg))
                component->ApplyAttributes();
            componentLatestData_.erase(current);
        }
    }
}

bool Connection::ProcessMessage(int msgID, MemoryBuffer& buffer)
{
    tempPacketCounter_.x_++;
    if (buffer.GetSize() == 0)
        return false;

    if (msgID != MSG_PACKED_MESSAGE)
    {
        ProcessUnknownMessage(msgID, buffer);
        return true;
    }

    while (!buffer.IsEof()) {
        msgID = buffer.ReadUInt();
        unsigned int packetSize = buffer.ReadUInt();
        MemoryBuffer msg(buffer.GetData() + buffer.GetPosition(), packetSize);
        buffer.Seek(buffer.GetPosition() + packetSize);

        switch (msgID)
        {
            case MSG_IDENTITY:
                ProcessIdentity(msgID, msg);
                break;

            case MSG_CONTROLS:
                ProcessControls(msgID, msg);
                break;

            case MSG_SCENELOADED:
                ProcessSceneLoaded(msgID, msg);
                break;

            case MSG_REQUESTPACKAGE:
            case MSG_PACKAGEDATA:
                ProcessPackageDownload(msgID, msg);
                break;

            case MSG_LOADSCENE:
                ProcessLoadScene(msgID, msg);
                break;

            case MSG_SCENECHECKSUMERROR:
                ProcessSceneChecksumError(msgID, msg);
                break;

            case MSG_CREATENODE:
            case MSG_NODEDELTAUPDATE:
            case MSG_NODELATESTDATA:
            case MSG_REMOVENODE:
            case MSG_CREATECOMPONENT:
            case MSG_COMPONENTDELTAUPDATE:
            case MSG_COMPONENTLATESTDATA:
            case MSG_REMOVECOMPONENT:
                ProcessSceneUpdate(msgID, msg);
                break;

            case MSG_REMOTEEVENT:
            case MSG_REMOTENODEEVENT:
                ProcessRemoteEvent(msgID, msg);
                break;

            case MSG_PACKAGEINFO:
                ProcessPackageInfo(msgID, msg);
                break;
            default:
                ProcessUnknownMessage(msgID, msg);
                break;
        }
    }
    return true;
}

void Connection::Ban()
{
    if (peer_)
    {
        peer_->AddToBanList(address_->ToString(false), 0);
    }
}

void Connection::ProcessLoadScene(int msgID, MemoryBuffer& msg)
{
    if (IsClient())
    {
        URHO3D_LOGWARNING("Received unexpected LoadScene message from client " + ToString());
        return;
    }

    if (!scene_)
    {
        URHO3D_LOGERROR("Can not handle LoadScene message without an assigned scene");
        return;
    }

    // Store the scene file name we need to eventually load
    sceneFileName_ = msg.ReadString();

    // Clear previous pending latest data and package downloads if any
    nodeLatestData_.clear();
    componentLatestData_.clear();
    downloads_.clear();

    // In case we have joined other scenes in this session, remove first all downloaded package files from the resource system
    // to prevent resource conflicts
    auto* cache = GetSubsystem<ResourceCache>();
    const ea::string& packageCacheDir = GetSubsystem<Network>()->GetPackageCacheDir();

    ea::vector<SharedPtr<PackageFile> > packages = cache->GetPackageFiles();
    for (unsigned i = 0; i < packages.size(); ++i)
    {
        PackageFile* package = packages[i];
        if (!package->GetName().find(packageCacheDir))
            cache->RemovePackageFile(package, true);
    }

    // Now check which packages we have in the resource cache or in the download cache, and which we need to download
    unsigned numPackages = msg.ReadVLE();
    if (!RequestNeededPackages(numPackages, msg))
    {
        OnSceneLoadFailed();
        return;
    }

    // If no downloads were queued, can load the scene directly
    if (downloads_.empty())
        OnPackagesReady();
}

void Connection::ProcessSceneChecksumError(int msgID, MemoryBuffer& msg)
{
    if (IsClient())
    {
        URHO3D_LOGWARNING("Received unexpected SceneChecksumError message from client " + ToString());
        return;
    }

    URHO3D_LOGERROR("Scene checksum error");
    OnSceneLoadFailed();
}

void Connection::ProcessSceneUpdate(int msgID, MemoryBuffer& msg)
{
    /// \todo On mobile devices processing this message may potentially cause a crash if it attempts to load new GPU resources
    /// while the application is minimized
    if (IsClient())
    {
        URHO3D_LOGWARNING("Received unexpected SceneUpdate message from client " + ToString());
        return;
    }

    if (!scene_)
        return;

    switch (msgID)
    {
    case MSG_CREATENODE:
        {
            unsigned nodeID = msg.ReadNetID();
            // In case of the root node (scene), it should already exist. Do not create in that case
            Node* node = scene_->GetNode(nodeID);
            if (!node)
            {
                // Add initially to the root level. May be moved as we receive the parent attribute
                node = scene_->CreateChild(nodeID, REPLICATED);
                // Create smoothed transform component
                node->CreateComponent<SmoothedTransform>(LOCAL);
            }

            // Read initial attributes, then snap the motion smoothing immediately to the end
            node->ReadDeltaUpdate(msg);
            auto* transform = node->GetComponent<SmoothedTransform>();
            if (transform)
                transform->Update(1.0f, 0.0f);

            // Read initial user variables
            unsigned numVars = msg.ReadVLE();
            while (numVars)
            {
                StringHash key = msg.ReadStringHash();
                node->SetVar(key, msg.ReadVariant());
                --numVars;
            }

            // Read components
            unsigned numComponents = msg.ReadVLE();
            while (numComponents)
            {
                --numComponents;

                StringHash type = msg.ReadStringHash();
                unsigned componentID = msg.ReadNetID();

                // Check if the component by this ID and type already exists in this node
                Component* component = scene_->GetComponent(componentID);
                if (!component || component->GetType() != type || component->GetNode() != node)
                {
                    if (component)
                        component->Remove();
                    component = node->CreateComponent(type, REPLICATED, componentID);
                }

                // If was unable to create the component, would desync the message and therefore have to abort
                if (!component)
                {
                    URHO3D_LOGERROR("CreateNode message parsing aborted due to unknown component");
                    return;
                }

                // Read initial attributes and apply
                component->ReadDeltaUpdate(msg);
                component->ApplyAttributes();
            }
        }
        break;

    case MSG_NODEDELTAUPDATE:
        {
            unsigned nodeID = msg.ReadNetID();
            Node* node = scene_->GetNode(nodeID);
            if (node)
            {
                node->ReadDeltaUpdate(msg);
                // ApplyAttributes() is deliberately skipped, as Node has no attributes that require late applying.
                // Furthermore it would propagate to components and child nodes, which is not desired in this case
                unsigned changedVars = msg.ReadVLE();
                while (changedVars)
                {
                    StringHash key = msg.ReadStringHash();
                    node->SetVar(key, msg.ReadVariant());
                    --changedVars;
                }
            }
            else
                URHO3D_LOGWARNING("NodeDeltaUpdate message received for missing node " + ea::to_string(nodeID));
        }
        break;

    case MSG_NODELATESTDATA:
        {
            unsigned nodeID = msg.ReadNetID();
            Node* node = scene_->GetNode(nodeID);
            if (node)
            {
                node->ReadLatestDataUpdate(msg);
                // ApplyAttributes() is deliberately skipped, as Node has no attributes that require late applying.
                // Furthermore it would propagate to components and child nodes, which is not desired in this case
            }
            else
            {
                // Latest data messages may be received out-of-order relative to node creation, so cache if necessary
                ea::vector<unsigned char>& data = nodeLatestData_[nodeID];
                data.resize(msg.GetSize());
                memcpy(&data[0], msg.GetData(), msg.GetSize());
            }
        }
        break;

    case MSG_REMOVENODE:
        {
            unsigned nodeID = msg.ReadNetID();
            Node* node = scene_->GetNode(nodeID);
            if (node)
                node->Remove();
            nodeLatestData_.erase(nodeID);
        }
        break;

    case MSG_CREATECOMPONENT:
        {
            unsigned nodeID = msg.ReadNetID();
            Node* node = scene_->GetNode(nodeID);
            if (node)
            {
                StringHash type = msg.ReadStringHash();
                unsigned componentID = msg.ReadNetID();

                // Check if the component by this ID and type already exists in this node
                Component* component = scene_->GetComponent(componentID);
                if (!component || component->GetType() != type || component->GetNode() != node)
                {
                    if (component)
                        component->Remove();
                    component = node->CreateComponent(type, REPLICATED, componentID);
                }

                // If was unable to create the component, would desync the message and therefore have to abort
                if (!component)
                {
                    URHO3D_LOGERROR("CreateComponent message parsing aborted due to unknown component");
                    return;
                }

                // Read initial attributes and apply
                component->ReadDeltaUpdate(msg);
                component->ApplyAttributes();
            }
            else
                URHO3D_LOGWARNING("CreateComponent message received for missing node " + ea::to_string(nodeID));
        }
        break;

    case MSG_COMPONENTDELTAUPDATE:
        {
            unsigned componentID = msg.ReadNetID();
            Component* component = scene_->GetComponent(componentID);
            if (component)
            {
                component->ReadDeltaUpdate(msg);
                component->ApplyAttributes();
            }
            else
                URHO3D_LOGWARNING("ComponentDeltaUpdate message received for missing component " + ea::to_string(componentID));
        }
        break;

    case MSG_COMPONENTLATESTDATA:
        {
            unsigned componentID = msg.ReadNetID();
            Component* component = scene_->GetComponent(componentID);
            if (component)
            {
                if (component->ReadLatestDataUpdate(msg))
                    component->ApplyAttributes();
            }
            else
            {
                // Latest data messages may be received out-of-order relative to component creation, so cache if necessary
                ea::vector<unsigned char>& data = componentLatestData_[componentID];
                data.resize(msg.GetSize());
                memcpy(&data[0], msg.GetData(), msg.GetSize());
            }
        }
        break;

    case MSG_REMOVECOMPONENT:
        {
            unsigned componentID = msg.ReadNetID();
            Component* component = scene_->GetComponent(componentID);
            if (component)
                component->Remove();
            componentLatestData_.erase(componentID);
        }
        break;

    default: break;
    }
}

void Connection::ProcessPackageDownload(int msgID, MemoryBuffer& msg)
{
    switch (msgID)
    {
    case MSG_REQUESTPACKAGE:
        if (!IsClient())
        {
            URHO3D_LOGWARNING("Received unexpected RequestPackage message from server");
            return;
        }
        else
        {
            ea::string name = msg.ReadString();

            if (!scene_)
            {
                URHO3D_LOGWARNING("Received a RequestPackage message without an assigned scene from client " + ToString());
                return;
            }

            // The package must be one of those required by the scene
            const ea::vector<SharedPtr<PackageFile> >& packages = scene_->GetRequiredPackageFiles();
            for (unsigned i = 0; i < packages.size(); ++i)
            {
                PackageFile* package = packages[i];
                const ea::string& packageFullName = package->GetName();
                if (!GetFileNameAndExtension(packageFullName).comparei(name))
                {
                    StringHash nameHash(name);

                    // Do not restart upload if already exists
                    if (uploads_.contains(nameHash))
                    {
                        URHO3D_LOGWARNING("Received a request for package " + name + " already in transfer");
                        return;
                    }

                    // Try to open the file now
                    SharedPtr<File> file(new File(context_, packageFullName));
                    if (!file->IsOpen())
                    {
                        URHO3D_LOGERROR("Failed to transmit package file " + name);
                        SendPackageError(name);
                        return;
                    }

                    URHO3D_LOGINFO("Transmitting package file " + name + " to client " + ToString());

                    uploads_[nameHash].file_ = file;
                    uploads_[nameHash].fragment_ = 0;
                    uploads_[nameHash].totalFragments_ = (file->GetSize() + PACKAGE_FRAGMENT_SIZE - 1) / PACKAGE_FRAGMENT_SIZE;
                    return;
                }
            }

            URHO3D_LOGERROR("Client requested an unexpected package file " + name);
            // Send the name hash only to indicate a failed download
            SendPackageError(name);
            return;
        }
        break;

    case MSG_PACKAGEDATA:
        if (IsClient())
        {
            URHO3D_LOGWARNING("Received unexpected PackageData message from client");
            return;
        }
        else
        {
            StringHash nameHash = msg.ReadStringHash();

            auto i = downloads_.find(nameHash);
            // In case of being unable to create the package file into the cache, we will still receive all data from the server.
            // Simply disregard it
            if (i == downloads_.end())
                return;

            PackageDownload& download = i->second;

            // If no further data, this is an error reply
            if (msg.IsEof())
            {
                OnPackageDownloadFailed(download.name_);
                return;
            }

            // If file has not yet been opened, try to open now. Prepend the checksum to the filename to allow multiple versions
            if (!download.file_)
            {
                download.file_ = new File(context_,
                    GetSubsystem<Network>()->GetPackageCacheDir() + ToStringHex(download.checksum_) + "_" + download.name_,
                    FILE_WRITE);
                if (!download.file_->IsOpen())
                {
                    OnPackageDownloadFailed(download.name_);
                    return;
                }
            }

            // Write the fragment data to the proper index
            unsigned char buffer[PACKAGE_FRAGMENT_SIZE];
            unsigned index = msg.ReadUInt();
            unsigned fragmentSize = msg.GetSize() - msg.GetPosition();

            msg.Read(buffer, fragmentSize);
            download.file_->Seek(index * PACKAGE_FRAGMENT_SIZE);
            download.file_->Write(buffer, fragmentSize);
            download.receivedFragments_.insert(index);

            // Check if all fragments received
            if (download.receivedFragments_.size() == download.totalFragments_)
            {
                URHO3D_LOGINFO("Package " + download.name_ + " downloaded successfully");

                // Instantiate the package and add to the resource system, as we will need it to load the scene
                download.file_->Close();
                GetSubsystem<ResourceCache>()->AddPackageFile(download.file_->GetName(), 0);

                // Then start the next download if there are more
                downloads_.erase(i);
                if (downloads_.empty())
                    OnPackagesReady();
                else
                {
                    PackageDownload& nextDownload = downloads_.begin()->second;

                    URHO3D_LOGINFO("Requesting package " + nextDownload.name_ + " from server");
                    msg_.Clear();
                    msg_.WriteString(nextDownload.name_);
                    SendMessage(MSG_REQUESTPACKAGE, true, true, msg_);
                    nextDownload.initiated_ = true;
                }
            }
        }
        break;

    default: break;
    }
}

void Connection::ProcessIdentity(int msgID, MemoryBuffer& msg)
{
    if (!IsClient())
    {
        URHO3D_LOGWARNING("Received unexpected Identity message from server");
        return;
    }

    identity_ = msg.ReadVariantMap();

    using namespace ClientIdentity;

    VariantMap eventData = identity_;
    eventData[P_CONNECTION] = this;
    eventData[P_ALLOW] = true;
    SendEvent(E_CLIENTIDENTITY, eventData);

    // If connection was denied as a response to the identity event, disconnect now
    if (!eventData[P_ALLOW].GetBool())
        Disconnect();
}

void Connection::ProcessControls(int msgID, MemoryBuffer& msg)
{
    if (!IsClient())
    {
        URHO3D_LOGWARNING("Received unexpected Controls message from server");
        return;
    }

    Controls newControls;
    newControls.buttons_ = msg.ReadUInt();
    newControls.yaw_ = msg.ReadFloat();
    newControls.pitch_ = msg.ReadFloat();
    newControls.extraData_ = msg.ReadVariantMap();

    SetControls(newControls);
    timeStamp_ = msg.ReadUByte();

    // Client may or may not send observer position & rotation for interest management
    if (!msg.IsEof())
        position_ = msg.ReadVector3();
    if (!msg.IsEof())
        rotation_ = msg.ReadPackedQuaternion();
}

void Connection::ProcessSceneLoaded(int msgID, MemoryBuffer& msg)
{
    if (!IsClient())
    {
        URHO3D_LOGWARNING("Received unexpected SceneLoaded message from server");
        return;
    }

    if (!scene_)
    {
        URHO3D_LOGWARNING("Received a SceneLoaded message without an assigned scene from client " + ToString());
        return;
    }

    unsigned checksum = msg.ReadUInt();

    if (checksum != scene_->GetChecksum())
    {
        URHO3D_LOGINFO("Scene checksum error from client " + ToString());
        msg_.Clear();
        SendMessage(MSG_SCENECHECKSUMERROR, true, true, msg_);
        OnSceneLoadFailed();
    }
    else
    {
        sceneLoaded_ = true;

        using namespace ClientSceneLoaded;

        VariantMap& eventData = GetEventDataMap();
        eventData[P_CONNECTION] = this;
        SendEvent(E_CLIENTSCENELOADED, eventData);
    }
}

void Connection::ProcessRemoteEvent(int msgID, MemoryBuffer& msg)
{
    using namespace RemoteEventData;

    if (msgID == MSG_REMOTEEVENT)
    {
        StringHash eventType = msg.ReadStringHash();
        if (!GetSubsystem<Network>()->CheckRemoteEvent(eventType))
        {
            URHO3D_LOGWARNING("Discarding not allowed remote event " + eventType.ToString());
            return;
        }

        VariantMap eventData = msg.ReadVariantMap();
        eventData[P_CONNECTION] = this;
        SendEvent(eventType, eventData);
    }
    else
    {
        if (!scene_)
        {
            URHO3D_LOGERROR("Can not receive remote node event without an assigned scene");
            return;
        }

        unsigned nodeID = msg.ReadNetID();
        StringHash eventType = msg.ReadStringHash();
        if (!GetSubsystem<Network>()->CheckRemoteEvent(eventType))
        {
            URHO3D_LOGWARNING("Discarding not allowed remote event " + eventType.ToString());
            return;
        }

        VariantMap eventData = msg.ReadVariantMap();
        Node* sender = scene_->GetNode(nodeID);
        if (!sender)
        {
            URHO3D_LOGWARNING("Missing sender for remote node event, discarding");
            return;
        }
        eventData[P_CONNECTION] = this;
        sender->SendEvent(eventType, eventData);
    }
}

Scene* Connection::GetScene() const
{
    return scene_;
}

bool Connection::IsConnected() const
{
    return peer_ && peer_->IsActive();
}

float Connection::GetRoundTripTime() const
{
    if (peer_)
    {
        SLNet::RakNetStatistics stats{};
        if (peer_->GetStatistics(address_->systemAddress, &stats))
            return (float)peer_->GetAveragePing(*address_);
    }
    return 0.0f;
}

unsigned Connection::GetLastHeardTime() const
{
    return const_cast<Timer&>(lastHeardTimer_).GetMSec(false);
}

float Connection::GetBytesInPerSec() const
{
    if (peer_)
    {
        SLNet::RakNetStatistics stats{};
        if (peer_->GetStatistics(address_->systemAddress, &stats))
            return (float)stats.valueOverLastSecond[SLNet::ACTUAL_BYTES_RECEIVED];
    }
    return 0.0f;
}

float Connection::GetBytesOutPerSec() const
{
    if (peer_)
    {
        SLNet::RakNetStatistics stats{};
        if (peer_->GetStatistics(address_->systemAddress, &stats))
            return (float)stats.valueOverLastSecond[SLNet::ACTUAL_BYTES_SENT];
    }
    return 0.0f;
}

int Connection::GetPacketsInPerSec() const
{
    return packetCounter_.x_;
}

int Connection::GetPacketsOutPerSec() const
{
    return packetCounter_.y_;
}

ea::string Connection::ToString() const
{
    return GetAddress() + ":" + ea::to_string(GetPort());
}

unsigned Connection::GetNumDownloads() const
{
    return downloads_.size();
}

const ea::string& Connection::GetDownloadName() const
{
    for (auto i = downloads_.begin(); i !=
        downloads_.end(); ++i)
    {
        if (i->second.initiated_)
            return i->second.name_;
    }
    return EMPTY_STRING;
}

float Connection::GetDownloadProgress() const
{
    for (auto i = downloads_.begin(); i !=
        downloads_.end(); ++i)
    {
        if (i->second.initiated_)
            return (float)i->second.receivedFragments_.size() / (float)i->second.totalFragments_;
    }
    return 1.0f;
}

void Connection::SendPackageToClient(PackageFile* package)
{
    if (!scene_)
        return;

    if (!IsClient())
    {
        URHO3D_LOGERROR("SendPackageToClient can be called on the server only");
        return;
    }
    if (!package)
    {
        URHO3D_LOGERROR("Null package specified for SendPackageToClient");
        return;
    }

    msg_.Clear();

    ea::string filename = GetFileNameAndExtension(package->GetName());
    msg_.WriteString(filename);
    msg_.WriteUInt(package->GetTotalSize());
    msg_.WriteUInt(package->GetChecksum());
    SendMessage(MSG_PACKAGEINFO, true, true, msg_);
}

void Connection::ConfigureNetworkSimulator(int latencyMs, float packetLoss)
{
    if (peer_)
        peer_->ApplyNetworkSimulator(packetLoss, latencyMs, 0);
}

void Connection::SetPacketSizeLimit(int limit)
{
    packedMessageLimit_ = limit;
}

void Connection::HandleAsyncLoadFinished(StringHash eventType, VariantMap& eventData)
{
    sceneLoaded_ = true;

    // Clear all replicated nodes
    scene_->Clear(true, false);

    msg_.Clear();
    msg_.WriteUInt(scene_->GetChecksum());
    SendMessage(MSG_SCENELOADED, true, true, msg_);
}

void Connection::ProcessNode(unsigned nodeID)
{
    // Check that we have not already processed this due to dependency recursion
    if (!nodesToProcess_.erase(nodeID))
        return;

    // Find replication state for the node
    auto i = sceneState_.nodeStates_.find(nodeID);
    if (i != sceneState_.nodeStates_.end())
    {
        // Replication state found: the node is either be existing or removed
        Node* node = i->second.node_;
        if (!node)
        {
            msg_.Clear();
            msg_.WriteNetID(nodeID);

            // Note: we will send MSG_REMOVENODE redundantly for each node in the hierarchy, even if removing the root node
            // would be enough. However, this may be better due to the client not possibly having updated parenting
            // information at the time of receiving this message
            SendMessage(MSG_REMOVENODE, true, true, msg_);
            sceneState_.nodeStates_.erase(nodeID);
        }
        else
            ProcessExistingNode(node, i->second);
    }
    else
    {
        // Replication state not found: this is a new node
        Node* node = scene_->GetNode(nodeID);
        if (node)
            ProcessNewNode(node);
        else
        {
            // Did not find the new node (may have been created, then removed immediately): erase from dirty set.
            sceneState_.dirtyNodes_.erase(nodeID);
        }
    }
}

void Connection::ProcessNewNode(Node* node)
{
    // Process depended upon nodes first, if they are dirty
    const ea::vector<Node*>& dependencyNodes = node->GetDependencyNodes();
    for (auto i = dependencyNodes.begin(); i != dependencyNodes.end(); ++i)
    {
        unsigned nodeID = (*i)->GetID();
        if (sceneState_.dirtyNodes_.contains(nodeID))
            ProcessNode(nodeID);
    }

    msg_.Clear();
    msg_.WriteNetID(node->GetID());

    NodeReplicationState& nodeState = sceneState_.nodeStates_[node->GetID()];
    nodeState.connection_ = this;
    nodeState.sceneState_ = &sceneState_;
    nodeState.node_ = node;
    node->AddReplicationState(&nodeState);

    // Write node's attributes
    node->WriteInitialDeltaUpdate(msg_, timeStamp_);

    // Write node's user variables
    const VariantMap& vars = node->GetVars();
    msg_.WriteVLE(vars.size());
    for (auto i = vars.begin(); i != vars.end(); ++i)
    {
        msg_.WriteStringHash(i->first);
        msg_.WriteVariant(i->second);
    }

    // Write node's components
    msg_.WriteVLE(node->GetNumNetworkComponents());
    const ea::vector<SharedPtr<Component> >& components = node->GetComponents();
    for (unsigned i = 0; i < components.size(); ++i)
    {
        Component* component = components[i];
        // Check if component is not to be replicated
        if (!component->IsReplicated())
            continue;

        ComponentReplicationState& componentState = nodeState.componentStates_[component->GetID()];
        componentState.connection_ = this;
        componentState.nodeState_ = &nodeState;
        componentState.component_ = component;
        component->AddReplicationState(&componentState);

        msg_.WriteStringHash(component->GetType());
        msg_.WriteNetID(component->GetID());
        component->WriteInitialDeltaUpdate(msg_, timeStamp_);
    }

    SendMessage(MSG_CREATENODE, true, true, msg_);

    nodeState.markedDirty_ = false;
    sceneState_.dirtyNodes_.erase(node->GetID());
}

void Connection::ProcessExistingNode(Node* node, NodeReplicationState& nodeState)
{
    // Process depended upon nodes first, if they are dirty
    const ea::vector<Node*>& dependencyNodes = node->GetDependencyNodes();
    for (auto i = dependencyNodes.begin(); i != dependencyNodes.end(); ++i)
    {
        unsigned nodeID = (*i)->GetID();
        if (sceneState_.dirtyNodes_.contains(nodeID))
            ProcessNode(nodeID);
    }

    // Check from the interest management component, if exists, whether should update
    /// \todo Searching for the component is a potential CPU hotspot. It should be cached
    auto* priority = node->GetComponent<NetworkPriority>();
    if (priority && (!priority->GetAlwaysUpdateOwner() || node->GetOwner() != this))
    {
        float distance = (node->GetWorldPosition() - position_).Length();
        if (!priority->CheckUpdate(distance, nodeState.priorityAcc_))
            return;
    }

    // Check if attributes have changed
    if (nodeState.dirtyAttributes_.Count() || nodeState.dirtyVars_.size())
    {
        const ea::vector<AttributeInfo>* attributes = node->GetNetworkAttributes();
        unsigned numAttributes = attributes->size();
        bool hasLatestData = false;

        for (unsigned i = 0; i < numAttributes; ++i)
        {
            if (nodeState.dirtyAttributes_.IsSet(i) && (attributes->at(i).mode_ & AttributeMode::LatestData))
            {
                hasLatestData = true;
                nodeState.dirtyAttributes_.Clear(i);
            }
        }

        // Send latestdata message if necessary
        if (hasLatestData)
        {
            msg_.Clear();
            msg_.WriteNetID(node->GetID());
            node->WriteLatestDataUpdate(msg_, timeStamp_);

            SendMessage(MSG_NODELATESTDATA, true, false, msg_, node->GetID());
        }

        // Send deltaupdate if remaining dirty bits, or vars have changed
        if (nodeState.dirtyAttributes_.Count() || nodeState.dirtyVars_.size())
        {
            msg_.Clear();
            msg_.WriteNetID(node->GetID());
            node->WriteDeltaUpdate(msg_, nodeState.dirtyAttributes_, timeStamp_);

            // Write changed variables
            msg_.WriteVLE(nodeState.dirtyVars_.size());
            const VariantMap& vars = node->GetVars();
            for (auto i = nodeState.dirtyVars_.begin(); i != nodeState.dirtyVars_.end(); ++i)
            {
                auto j = vars.find(*i);
                if (j != vars.end())
                {
                    msg_.WriteStringHash(j->first);
                    msg_.WriteVariant(j->second);
                }
                else
                {
                    // Variable has been marked dirty, but is removed (which is unsupported): send a dummy variable in place
                    URHO3D_LOGWARNING("Sending dummy user variable as original value was removed");
                    msg_.WriteStringHash(StringHash());
                    msg_.WriteVariant(Variant::EMPTY);
                }
            }

            SendMessage(MSG_NODEDELTAUPDATE, true, true, msg_);

            nodeState.dirtyAttributes_.ClearAll();
            nodeState.dirtyVars_.clear();
        }
    }

    // Check for removed or changed components
    for (auto i = nodeState.componentStates_.begin();
         i != nodeState.componentStates_.end();)
    {
        auto current = i++;
        ComponentReplicationState& componentState = current->second;
        Component* component = componentState.component_;
        if (!component)
        {
            // Removed component
            msg_.Clear();
            msg_.WriteNetID(current->first);

            SendMessage(MSG_REMOVECOMPONENT, true, true, msg_);
            nodeState.componentStates_.erase(current);
        }
        else
        {
            // Existing component. Check if attributes have changed
            if (componentState.dirtyAttributes_.Count())
            {
                const ea::vector<AttributeInfo>* attributes = component->GetNetworkAttributes();
                unsigned numAttributes = attributes->size();
                bool hasLatestData = false;

                for (unsigned i = 0; i < numAttributes; ++i)
                {
                    if (componentState.dirtyAttributes_.IsSet(i) && (attributes->at(i).mode_ & AttributeMode::LatestData))
                    {
                        hasLatestData = true;
                        componentState.dirtyAttributes_.Clear(i);
                    }
                }

                // Send latestdata message if necessary
                if (hasLatestData)
                {
                    msg_.Clear();
                    msg_.WriteNetID(component->GetID());
                    component->WriteLatestDataUpdate(msg_, timeStamp_);

                    SendMessage(MSG_COMPONENTLATESTDATA, true, false, msg_, component->GetID());
                }

                // Send deltaupdate if remaining dirty bits
                if (componentState.dirtyAttributes_.Count())
                {
                    msg_.Clear();
                    msg_.WriteNetID(component->GetID());
                    component->WriteDeltaUpdate(msg_, componentState.dirtyAttributes_, timeStamp_);

                    SendMessage(MSG_COMPONENTDELTAUPDATE, true, true, msg_);

                    componentState.dirtyAttributes_.ClearAll();
                }
            }
        }
    }

    // Check for new components
    if (nodeState.componentStates_.size() != node->GetNumNetworkComponents())
    {
        const ea::vector<SharedPtr<Component> >& components = node->GetComponents();
        for (unsigned i = 0; i < components.size(); ++i)
        {
            Component* component = components[i];
            // Check if component is not to be replicated
            if (!component->IsReplicated())
                continue;

            auto j = nodeState.componentStates_.find(
                component->GetID());
            if (j == nodeState.componentStates_.end())
            {
                // New component
                ComponentReplicationState& componentState = nodeState.componentStates_[component->GetID()];
                componentState.connection_ = this;
                componentState.nodeState_ = &nodeState;
                componentState.component_ = component;
                component->AddReplicationState(&componentState);

                msg_.Clear();
                msg_.WriteNetID(node->GetID());
                msg_.WriteStringHash(component->GetType());
                msg_.WriteNetID(component->GetID());
                component->WriteInitialDeltaUpdate(msg_, timeStamp_);

                SendMessage(MSG_CREATECOMPONENT, true, true, msg_);
            }
        }
    }

    nodeState.markedDirty_ = false;
    sceneState_.dirtyNodes_.erase(node->GetID());
}

bool Connection::RequestNeededPackages(unsigned numPackages, MemoryBuffer& msg)
{
    auto* cache = GetSubsystem<ResourceCache>();
    const ea::string& packageCacheDir = GetSubsystem<Network>()->GetPackageCacheDir();

    ea::vector<SharedPtr<PackageFile> > packages = cache->GetPackageFiles();
    ea::vector<ea::string> downloadedPackages;
    bool packagesScanned = false;

    for (unsigned i = 0; i < numPackages; ++i)
    {
        ea::string name = msg.ReadString();
        unsigned fileSize = msg.ReadUInt();
        unsigned checksum = msg.ReadUInt();
        ea::string checksumString = ToStringHex(checksum);
        bool found = false;

        // Check first the resource cache
        for (unsigned j = 0; j < packages.size(); ++j)
        {
            PackageFile* package = packages[j];
            if (!GetFileNameAndExtension(package->GetName()).comparei(name) && package->GetTotalSize() == fileSize &&
                package->GetChecksum() == checksum)
            {
                found = true;
                break;
            }
        }

        if (found)
            continue;

        if (!packagesScanned)
        {
            if (packageCacheDir.empty())
            {
                URHO3D_LOGERROR("Can not check/download required packages, as package cache directory is not set");
                return false;
            }

            GetSubsystem<FileSystem>()->ScanDir(downloadedPackages, packageCacheDir, "*.*", SCAN_FILES, false);
            packagesScanned = true;
        }

        // Then the download cache
        for (unsigned j = 0; j < downloadedPackages.size(); ++j)
        {
            const ea::string& fileName = downloadedPackages[j];
            // In download cache, package file name format is checksum_packagename
            if (!fileName.find(checksumString) && !fileName.substr(9).comparei(name))
            {
                // Name matches. Check file size and actual checksum to be sure
                SharedPtr<PackageFile> newPackage(new PackageFile(context_, packageCacheDir + fileName));
                if (newPackage->GetTotalSize() == fileSize && newPackage->GetChecksum() == checksum)
                {
                    // Add the package to the resource system now, as we will need it to load the scene
                    cache->AddPackageFile(newPackage, 0);
                    found = true;
                    break;
                }
            }
        }

        // Package not found, need to request a download
        if (!found)
            RequestPackage(name, fileSize, checksum);
    }

    return true;
}

void Connection::RequestPackage(const ea::string& name, unsigned fileSize, unsigned checksum)
{
    StringHash nameHash(name);
    if (downloads_.contains(nameHash))
        return; // Download already exists

    PackageDownload& download = downloads_[nameHash];
    download.name_ = name;
    download.totalFragments_ = (fileSize + PACKAGE_FRAGMENT_SIZE - 1) / PACKAGE_FRAGMENT_SIZE;
    download.checksum_ = checksum;

    // Start download now only if no existing downloads, else wait for the existing ones to finish
    if (downloads_.size() == 1)
    {
        URHO3D_LOGINFO("Requesting package " + name + " from server");
        msg_.Clear();
        msg_.WriteString(name);
        SendMessage(MSG_REQUESTPACKAGE, true, true, msg_);
        download.initiated_ = true;
    }
}

void Connection::SendPackageError(const ea::string& name)
{
    msg_.Clear();
    msg_.WriteStringHash(name);
    SendMessage(MSG_PACKAGEDATA, true, false, msg_);
}

void Connection::OnSceneLoadFailed()
{
    sceneLoaded_ = false;

    using namespace NetworkSceneLoadFailed;

    VariantMap& eventData = GetEventDataMap();
    eventData[P_CONNECTION] = this;
    SendEvent(E_NETWORKSCENELOADFAILED, eventData);
}

void Connection::OnPackageDownloadFailed(const ea::string& name)
{
    URHO3D_LOGERROR("Download of package " + name + " failed");
    // As one package failed, we can not join the scene in any case. Clear the downloads
    downloads_.clear();
    OnSceneLoadFailed();
}

void Connection::OnPackagesReady()
{
    if (!scene_)
        return;

    // If sceneLoaded_ is true, we may have received additional package downloads while already joined in a scene.
    // In that case the scene should not be loaded.
    if (sceneLoaded_)
        return;

    if (sceneFileName_.empty())
    {
        // If the scene filename is empty, just clear the scene of all existing replicated content, and send the loaded reply
        scene_->Clear(true, false);
        sceneLoaded_ = true;

        msg_.Clear();
        msg_.WriteUInt(scene_->GetChecksum());
        SendMessage(MSG_SCENELOADED, true, true, msg_);
    }
    else
    {
        // Otherwise start the async loading process
        ea::string extension = GetExtension(sceneFileName_);
        SharedPtr<File> file = GetSubsystem<ResourceCache>()->GetFile(sceneFileName_);
        bool success;

        if (extension == ".xml")
            success = scene_->LoadAsyncXML(file);
        else
            success = scene_->LoadAsync(file);

        if (!success)
            OnSceneLoadFailed();
    }
}

void Connection::ProcessPackageInfo(int msgID, MemoryBuffer& msg)
{
    if (!scene_)
        return;

    if (IsClient())
    {
        URHO3D_LOGWARNING("Received unexpected packages info message from client");
        return;
    }

    RequestNeededPackages(1, msg);
}

void Connection::ProcessUnknownMessage(int msgID, MemoryBuffer& msg)
{
    // If message was not handled internally, forward as an event
    using namespace NetworkMessage;

    VariantMap& eventData = GetEventDataMap();
    eventData[P_CONNECTION] = this;
    eventData[P_MESSAGEID] = (int)msgID;
    eventData[P_DATA].SetBuffer(msg.GetData(), msg.GetSize());
    SendEvent(E_NETWORKMESSAGE, eventData);
}

ea::string Connection::GetAddress() const {
    return ea::string(address_->ToString(false /*write port*/));
}

void Connection::SetAddressOrGUID(const SLNet::AddressOrGUID& addr)
{
    delete address_;
    address_ = nullptr;
    address_ = new SLNet::AddressOrGUID(addr);
}

}
