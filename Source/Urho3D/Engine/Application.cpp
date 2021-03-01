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

#include "../Core/Profiler.h"
#include "../Engine/Application.h"
#include "../Engine/EngineDefs.h"
#include "../Engine/EngineEvents.h"
#include "../IO/IOEvents.h"
#include "../IO/Log.h"
#if URHO3D_CSHARP
#include "../Script/Script.h"
#endif

#if defined(IOS) || defined(TVOS)
#include "../Graphics/Graphics.h"
#include <SDL/SDL.h>
#endif
#include "../Core/CommandLine.h"

#include "../DebugNew.h"

namespace Urho3D
{

#if defined(IOS) || defined(TVOS) || defined(__EMSCRIPTEN__)
// Code for supporting SDL_iPhoneSetAnimationCallback() and emscripten_set_main_loop_arg()
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif
void RunFrame(void* data)
{
    static_cast<Engine*>(data)->RunFrame();
}
#endif

static int(__cdecl*CustomMain)() = nullptr;

void Application::SetCustomMainFunction(void* customMain)
{
    CustomMain = (decltype(CustomMain))customMain;
}

#if MOBILE && !URHO3D_STATIC
// Android and iOS may use a custom entry point written in C#. Allow specifying a custom entry point without having to use
// URHO3D_DEFINE_APPLICATION_MAIN() macro in native code.
static int RunApplication()
{
    assert(CustomMain != nullptr);
    return CustomMain();
}
URHO3D_DEFINE_APPLICATION_MAIN(RunApplication());
#endif

#if DESKTOP
/// Command line parser.
static CLI::App commandLine_{};
#endif

Application::Application(Context* context) :
    Object(context),
    exitCode_(EXIT_SUCCESS)
{
    // Create the Engine, but do not initialize it yet. Subsystems except Graphics & Renderer are registered at this point
    engine_ = new Engine(context);

    // Subscribe to log messages so that can show errors if ErrorExit() is called with empty message
    SubscribeToEvent(E_LOGMESSAGE, URHO3D_HANDLER(Application, HandleLogMessage));
}

int Application::Run()
{
    // Profiler requires main thread to be named "Main" as fps calculations depend on it.
    URHO3D_PROFILE_THREAD("Main");
#if !defined(__GNUC__) || __EXCEPTIONS
    try
    {
#endif
        // Register application command line arguments or set up engine parameters
        Setup();

#if DESKTOP
        if (engine_->GetParameter(engineParameters_, EP_ENGINE_CLI_PARAMETERS, true).GetBool())
        {
            // Register engine command line arguments
            Engine::DefineParameters(commandLine_, engineParameters_);
        }
#endif

        if (exitCode_)
            return exitCode_;

#if DESKTOP
        // Parse command line parameters
        {
            const StringVector& rawArguments = GetArguments();
            std::vector<std::string> cliArgs;
            cliArgs.reserve(rawArguments.size());
            // CLI11 detail - arguments must be in reversed order.
            for (auto i = static_cast<int>(rawArguments.size() - 1); i >= 0; i--)
                cliArgs.emplace_back(rawArguments[static_cast<unsigned>(i)].c_str());

            try {
                commandLine_.parse(cliArgs);
            } catch(const CLI::ParseError &e) {
                exitCode_ = commandLine_.exit(e);
                return exitCode_;
            }
        }
#endif

        if (!engine_->Initialize(engineParameters_))
        {
            ErrorExit();
            return exitCode_;
        }

#if URHO3D_PLUGINS && URHO3D_CSHARP
        if (engine_->GetParameter(engineParameters_, EP_ENGINE_AUTO_LOAD_SCRIPTS, true).GetBool())
        {
            if (auto* api = Script::GetRuntimeApi())
            {
                scriptsPlugin_ = api->CompileResourceScriptPlugin();
                if (scriptsPlugin_.NotNull())
                {
                    scriptsPlugin_->SendEvent(E_PLUGINLOAD);
                    scriptsPlugin_->Load();
                    scriptsPlugin_->SendEvent(E_PLUGINSTART);
                    scriptsPlugin_->Start();
                }
            }
        }
#endif

        Start();
        if (exitCode_ || engine_->IsExiting())
        {
            Stop();
            return exitCode_;
        }

        SendEvent(E_APPLICATIONSTARTED);

        // Platforms other than iOS/tvOS and Emscripten run a blocking main loop
#if !defined(IOS) && !defined(TVOS) && !defined(__EMSCRIPTEN__)
        while (!engine_->IsExiting())
            engine_->RunFrame();

#if URHO3D_PLUGINS && URHO3D_CSHARP
        if (scriptsPlugin_.NotNull())
        {
            scriptsPlugin_->SendEvent(E_PLUGINSTOP);
            scriptsPlugin_->Stop();
        }
#endif

        Stop();

#if URHO3D_PLUGINS && URHO3D_CSHARP
        if (scriptsPlugin_.NotNull())
        {
            scriptsPlugin_->SendEvent(E_PLUGINUNLOAD);
            scriptsPlugin_->Unload();
            Script::GetRuntimeApi()->DereferenceAndDispose(scriptsPlugin_.Detach());
        }
#endif

        // iOS/tvOS will setup a timer for running animation frames so eg. Game Center can run. In this case we do not
        // support calling the Stop() function, as the application will never stop manually
#else
#if defined(IOS) || defined(TVOS)
        SDL_iPhoneSetAnimationCallback(GetSubsystem<Graphics>()->GetWindow(), 1, &RunFrame, engine_);
#elif defined(__EMSCRIPTEN__)
        emscripten_set_main_loop_arg(RunFrame, engine_, 0, 1);
#endif
#endif

        return exitCode_;
#if !defined(__GNUC__) || __EXCEPTIONS
    }
    catch (std::bad_alloc&)
    {
        ErrorDialog(GetTypeName(), "An out-of-memory error occurred. The application will now exit.");
        return EXIT_FAILURE;
    }
#endif
}

void Application::ErrorExit(const ea::string& message)
{
    engine_->Exit(); // Close the rendering window
    exitCode_ = EXIT_FAILURE;

    std::function<void(const ea::string&)> showError;
    if (engineParameters_[EP_HEADLESS].GetBool())
    {
        showError = [this](const ea::string& message)
        {
            URHO3D_LOGERROR(message.c_str());
        };
    }
    else
    {
        showError = [this](const ea::string& message)
        {
            ErrorDialog(GetTypeName(), message);
        };
    }

    if (!message.length())
    {
        showError(startupErrors_.length() ?
            startupErrors_ : ea::string("Application has been terminated due to unexpected error."));
    }
    else
        showError(message);
}

void Application::HandleLogMessage(StringHash eventType, VariantMap& eventData)
{
    using namespace LogMessage;

    if (eventData[P_LEVEL].GetInt() == LOG_ERROR)
    {
        // Strip the timestamp if necessary
        ea::string error = eventData[P_MESSAGE].GetString();
        unsigned bracketPos = error.find(']');
        if (bracketPos != ea::string::npos)
            error = error.substr(bracketPos + 2);

        startupErrors_ += error + "\n";
    }
}
#if DESKTOP
CLI::App& Application::GetCommandLineParser()
{
    return commandLine_;
}
#endif

}
