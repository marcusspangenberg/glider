#include "FileBrowser.h"

#include <algorithm>
#include <cstdlib>
#include <imgui.h>
#include <ranges>
#include <string>

std::filesystem::path FileBrowser::defaultStartDir()
{
    const char* home = std::getenv("HOME");
    if (home && std::filesystem::is_directory(home))
    {
        return std::filesystem::path(home);
    }
    return std::filesystem::current_path();
}

void FileBrowser::openForOpen(const std::string& title, Callback onConfirm)
{
    mode_ = FileBrowserMode::open;
    popupTitle_ = title;
    callback_ = std::move(onConfirm);
    filenameBuf_[0] = '\0';
    selectedIndex_ = -1;
    errorMessage_.clear();

    std::filesystem::path startDir = lastDir_.empty() ? defaultStartDir() : lastDir_;
    navigateTo(startDir);
    pendingOpen_ = true;
}

void FileBrowser::openForSave(const std::string& title, const std::string& initialPath, Callback onConfirm)
{
    mode_ = FileBrowserMode::saveAs;
    popupTitle_ = title;
    callback_ = std::move(onConfirm);
    selectedIndex_ = -1;
    errorMessage_.clear();

    std::filesystem::path startDir;
    if (!initialPath.empty())
    {
        std::filesystem::path p(initialPath);
        startDir = p.parent_path();
        std::string filename = p.filename().string();
        strncpy(filenameBuf_, filename.c_str(), sizeof(filenameBuf_) - 1);
        filenameBuf_[sizeof(filenameBuf_) - 1] = '\0';
    }
    else
    {
        filenameBuf_[0] = '\0';
        startDir = lastDir_.empty() ? defaultStartDir() : lastDir_;
    }

    navigateTo(startDir);
    pendingOpen_ = true;
}

void FileBrowser::navigateTo(const std::filesystem::path& dir)
{
    errorMessage_.clear();
    try
    {
        std::filesystem::path canonical = std::filesystem::canonical(dir);
        currentDir_ = canonical;
        lastDir_ = canonical;
    }
    catch (const std::filesystem::filesystem_error&)
    {
        currentDir_ = dir;
    }
    refreshEntries();
    selectedIndex_ = -1;
}

void FileBrowser::refreshEntries()
{
    entries_.clear();
    errorMessage_.clear();

    std::vector<Entry> dirs;
    std::vector<Entry> files;

    try
    {
        for (const auto& entry : std::filesystem::directory_iterator(currentDir_))
        {
            std::string name = entry.path().filename().string();
            if (name.empty() || name[0] == '.')
            {
                continue;
            }

            if (entry.is_directory())
            {
                dirs.push_back({name, true});
            }
            else if (entry.is_regular_file())
            {
                files.push_back({name, false});
            }
        }
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        errorMessage_ = e.what();
    }

    auto caseInsensitiveLess = [](const Entry& a, const Entry& b) {
        std::string la = a.name;
        std::string lb = b.name;
        std::ranges::transform(la, la.begin(), [](unsigned char c) { return std::tolower(c); });
        std::ranges::transform(lb, lb.begin(), [](unsigned char c) { return std::tolower(c); });
        return la < lb;
    };

    std::ranges::sort(dirs, caseInsensitiveLess);
    std::ranges::sort(files, caseInsensitiveLess);

    entries_.insert(entries_.end(), dirs.begin(), dirs.end());
    entries_.insert(entries_.end(), files.begin(), files.end());
}

void FileBrowser::draw()
{
    if (pendingOpen_)
    {
        ImGui::OpenPopup(popupTitle_.c_str());
        pendingOpen_ = false;
    }

    ImGui::SetNextWindowSize(ImVec2(560.0f, 460.0f), ImGuiCond_Always);
    if (ImGui::BeginPopupModal(popupTitle_.c_str(), nullptr, ImGuiWindowFlags_NoResize))
    {
        drawContents();
        if (pendingClose_)
        {
            pendingClose_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void FileBrowser::drawContents()
{
    // Current directory path
    ImGui::TextUnformatted(currentDir_.string().c_str());

    ImGui::SameLine();

    bool atRoot = (currentDir_ == currentDir_.root_path());
    if (atRoot)
    {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Up"))
    {
        navigateTo(currentDir_.parent_path());
    }
    if (atRoot)
    {
        ImGui::EndDisabled();
    }

    ImGui::Separator();

    // Directory/file list
    ImGui::BeginChild("##entries", ImVec2(0.0f, 280.0f), true);
    for (int32_t i = 0; i < static_cast<int32_t>(entries_.size()); ++i)
    {
        const Entry& entry = entries_[i];
        std::string label = entry.isDirectory ? "[D] " + entry.name : entry.name;

        bool selected = (selectedIndex_ == i);
        if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick))
        {
            selectedIndex_ = i;

            if (!entry.isDirectory && mode_ == FileBrowserMode::open)
            {
                strncpy(filenameBuf_, entry.name.c_str(), sizeof(filenameBuf_) - 1);
                filenameBuf_[sizeof(filenameBuf_) - 1] = '\0';
            }

            if (ImGui::IsMouseDoubleClicked(0))
            {
                if (entry.isDirectory)
                {
                    navigateTo(currentDir_ / entry.name);
                }
                else if (mode_ == FileBrowserMode::open && filenameBuf_[0] != '\0')
                {
                    std::filesystem::path result = currentDir_ / filenameBuf_;
                    callback_(result.string());
                    ImGui::CloseCurrentPopup();
                }
            }
        }
    }
    ImGui::EndChild();

    // Filename input
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("##filename", filenameBuf_, sizeof(filenameBuf_));

    // Error message
    if (!errorMessage_.empty())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", errorMessage_.c_str());
    }

    ImGui::Spacing();

    // Confirm / Cancel buttons
    const char* confirmLabel = (mode_ == FileBrowserMode::open) ? "Open" : "Save";
    bool canConfirm = (filenameBuf_[0] != '\0');
    if (!canConfirm)
    {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button(confirmLabel, ImVec2(120.0f, 0.0f)))
    {
        std::filesystem::path result = currentDir_ / filenameBuf_;
        if (mode_ == FileBrowserMode::saveAs && std::filesystem::exists(result))
        {
            pendingOverwritePath_ = result;
            pendingOverwriteConfirm_ = true;
            ImGui::OpenPopup("Overwrite?");
        }
        else
        {
            callback_(result.string());
            ImGui::CloseCurrentPopup();
        }
    }

    drawOverwriteConfirm();
    if (!canConfirm)
    {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
    {
        ImGui::CloseCurrentPopup();
    }
}

void FileBrowser::drawOverwriteConfirm()
{
    if (!pendingOverwriteConfirm_)
    {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f), ImGuiCond_Always);
    if (ImGui::BeginPopupModal("Overwrite?", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
    {
        ImGui::TextWrapped("'%s' already exists. Overwrite?", pendingOverwritePath_.filename().string().c_str());
        ImGui::Spacing();

        if (ImGui::Button("Overwrite", ImVec2(120.0f, 0.0f)))
        {
            pendingOverwriteConfirm_ = false;
            pendingClose_ = true;
            callback_(pendingOverwritePath_.string());
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
        {
            pendingOverwriteConfirm_ = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
