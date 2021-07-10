
#include <unistd.h>
#include "nfd.h"
#include "nfd_common.h"

#include <string>
#include <vector>
#include <cstring>
#include <algorithm>
#include <nfd.h>


namespace nfd
{

std::vector <std::string> split(std::string data, const std::string& token)
{
    std::vector <std::string> output;
    size_t pos = std::string::npos;
    do
    {
        pos = data.find(token);
        output.push_back(data.substr(0, pos));
        if (std::string::npos != pos)
            data = data.substr(pos + token.size());
    } while (std::string::npos != pos);
    return output;
}

std::string replace(std::string input, const std::string& find, const std::string& replace)
{
    while (input.find(find) != std::string::npos)
    {
        input.replace(input.find(find), find.size(), replace);
    }
    return input;
}

void rtrim(std::string& value)
{
    value.erase(std::find_if(value.rbegin(), value.rend(),
        [](int ch) { return !std::isspace(ch); }).base(), value.end()
   );
}

std::string popen(const std::string& command, const std::vector <std::string>& args = { },
    const std::string& currentDir = "")
{
    std::string lastDir;
    char buffer[1024];

    if (!currentDir.empty())
    {
        if (getcwd(buffer, sizeof(buffer)) == nullptr)
            return "";
        lastDir = buffer;
        chdir(currentDir.c_str());
    }

    std::string commandLine = "\"" + nfd::replace(command, "\"", "\\\"") + "\" ";

    for (const auto& arg: args)
    {
        commandLine.append("\"");
        commandLine.append(nfd::replace(arg, "\"", "\\\""));
        commandLine.append("\" ");
    }

    std::string output;
    FILE* stream = ::popen(commandLine.c_str(), "r");
    while (fgets(buffer, sizeof(buffer), stream) != nullptr)
        output.append(buffer);
    pclose(stream);

    if (!currentDir.empty())
        chdir(lastDir.c_str());

    return output;
}

struct DialogHandlerDetector
{
    bool usekDialog;

    DialogHandlerDetector() : usekDialog(false)
    {
        const char* desktop = getenv("XDG_SESSION_DESKTOP");
        if (desktop == "KDE" || desktop == "lxqt")
            usekDialog = !nfd::popen("kdialog", {"--help"}).empty();
    }
};

bool use_kdialog()
{
    static DialogHandlerDetector detector;
    return detector.usekDialog;
}

std::string kdialog_convert_filters(const nfdchar_t* filterList)
{
    // Convert from "png,jpg;pdf"
    // Convert to "Images(*.png *.jpg)|XML Files(*.xml)"
    std::string result;
    auto groups = nfd::split(filterList ? filterList : "*", ";");
    for (auto i = 0; i < groups.size(); i++)
    {
        auto parts = nfd::split(groups[i], ",");
        std::string extensions;
        for (auto j = 0; j < parts.size(); j++)
        {
            if (j > 0)
                extensions += " ";
            extensions += "*.";
            extensions += parts[j];
        }
        if (i > 0)
            result += "|";
        result += extensions;   // Use list of extensions as filter label.
        result += "(";
        result += extensions;
        result += ")";
    }
    return result;
}

void zenity_convert_filters(std::vector<std::string>& args, const nfdchar_t* filterList)
{
    // Convert from "png,jpg;pdf"
    // Convert to --file-filter "*.png *.xpm *.jpg" --file-filter "*.xml"
    auto groups = nfd::split(filterList ? filterList : "*", ";");
    for (auto group : groups)
    {
        group = "*." + nfd::replace(group, ",", " *.");
        args.emplace_back("--file-filter");
        args.emplace_back(group);
    }
}

}

extern "C"
{

/* single file open dialog */
nfdresult_t NFD_OpenDialog(const nfdchar_t* filterList, const nfdchar_t* defaultPath, nfdchar_t** outPath, void* owner)
{
    std::string result;
    if (nfd::use_kdialog())
    {
        if (!defaultPath)
            defaultPath = ".";
        auto filters = nfd::kdialog_convert_filters(filterList);
        result = nfd::popen("kdialog", {"--getopenfilename", /*"--title", title,*/ defaultPath, filters});
    }
    else
    {
        std::vector<std::string> args = {/*"--title", title,*/ "--file-selection"};
        if (defaultPath)
        {
            args.emplace_back("--filename");
            args.emplace_back(defaultPath);
        }
        nfd::zenity_convert_filters(args, filterList);
        result = nfd::popen("zenity", args, defaultPath);
    }

    nfd::rtrim(result);
    if (result.empty())
    {
        NFDi_SetError("Unknown error.");
        return NFD_ERROR;
    }
    else
    {
        *outPath = (nfdchar_t*)NFDi_Malloc(result.length() + 1);
        strcpy(*outPath, result.c_str());
        return NFD_OKAY;
    }
}

/* multiple file open dialog */
nfdresult_t NFD_OpenDialogMultiple(const nfdchar_t* filterList, const nfdchar_t* defaultPath, nfdpathset_t* outPaths)
{
    std::vector<std::string> results;
    if (nfd::use_kdialog())
    {
        if (!defaultPath)
            defaultPath = ".";

        auto filters = nfd::kdialog_convert_filters(filterList);
        std::string dirsString = nfd::popen("kdialog", {"--getopenfilename", /*"--title", title,*/ "--multiple",
                                                        defaultPath, filters});
        // Trim whitespace from the end.
        nfd::rtrim(dirsString);
        // BUG: kdialog is stupid enough to use space between filenames. We do best we can to correctly split paths,
        // but if someone creates file or folder with space as a first character this will return incorrect results.
        results = nfd::split(dirsString, " /");
        if (results.size() > 1)
        {
            // Prepend forward-slashes that were lost during splitting.
            for (auto it = results.begin() + 1; it != results.end(); it++)
                *it = "/" + *it;
        }
    }
    else
    {
        std::vector<std::string> args = {/*"--title", title,*/ "--file-selection", "--multiple", "--separator", ":"};
        if (defaultPath)
        {
            args.emplace_back("--filename");
            args.emplace_back(defaultPath);
        }
        nfd::zenity_convert_filters(args, filterList);
        results =  nfd::split(nfd::popen("zenity", args), ":");
    }

    if (results.empty())
    {
        NFDi_SetError("Unknown error.");
        return NFD_ERROR;
    }
    else
    {
        outPaths->count = results.size();
        outPaths->indices = static_cast<size_t*>(NFDi_Malloc(sizeof(size_t) * results.size()));
        size_t buf_size = 0;
        for (const auto& path : results)
            buf_size += path.size() + 1;
        outPaths->buf = static_cast<nfdchar_t*>(NFDi_Malloc(buf_size));
        size_t offset = 0;
        for (auto i = 0; i < results.size(); i++)
        {
            const auto& path = results[i];
            outPaths->indices[i] = offset;
            strcpy(&outPaths->buf[offset], path.c_str());
            offset += path.length() + 1;
        }
        return NFD_OKAY;
    }
}

/* save dialog */
nfdresult_t NFD_SaveDialog(const nfdchar_t* filterList, const nfdchar_t* defaultPath, nfdchar_t** outPath, void* owner)
{
    std::string result;
    if (nfd::use_kdialog())
    {
        if (!defaultPath)
            defaultPath = ".";
        auto filters = nfd::kdialog_convert_filters(filterList);
        result = nfd::popen("kdialog", {"--getsavefilename", /*"--title", title,*/ defaultPath, filters});
    }
    else
    {
        std::vector<std::string> args = {/*"--title", title,*/ "--file-selection", "--save", "--confirm-overwrite"};
        if (defaultPath)
        {
            args.emplace_back("--filename");
            args.emplace_back(defaultPath);
        }
        nfd::zenity_convert_filters(args, filterList);
        result = nfd::popen("zenity", args);
    }

    nfd::rtrim(result);
    if (result.empty())
    {
        NFDi_SetError("Unknown error.");
        return NFD_ERROR;
    }
    else
    {
        *outPath = (nfdchar_t*)NFDi_Malloc(result.length() + 1);
        strcpy(*outPath, result.c_str());
        return NFD_OKAY;
    }
}

/* select folder dialog */
nfdresult_t NFD_PickFolder(const nfdchar_t* defaultPath, nfdchar_t** outPath)
{
    if (!defaultPath)
        defaultPath = ".";
    std::string result;
    if (nfd::use_kdialog())
        result = nfd::popen("kdialog", {"--getexistingdirectory", /*"--title", title,*/ defaultPath});
    else
        result = nfd::popen("zenity", {/*"--title", title,*/ "--file-selection", "--directory"}, defaultPath);

    nfd::rtrim(result);
    if (result.empty())
    {
        NFDi_SetError("Unknown error.");
        return NFD_ERROR;
    }
    else
    {
        *outPath = (nfdchar_t*)NFDi_Malloc(result.length() + 1);
        strcpy(*outPath, result.c_str());
        return NFD_OKAY;
    }
}

}
