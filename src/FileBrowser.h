#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

enum class FileBrowserMode
{
    open,
    saveAs
};

class FileBrowser
{
public:
    using Callback = std::function<void(const std::string&)>;

    void openForOpen(const std::string& title, Callback onConfirm);
    void openForSave(const std::string& title, const std::string& initialPath, Callback onConfirm);
    void draw();

private:
    struct Entry
    {
        std::string name;
        bool isDirectory;
    };

    FileBrowserMode mode_ = FileBrowserMode::open;
    std::string popupTitle_;
    Callback callback_;

    std::filesystem::path currentDir_;
    std::filesystem::path lastDir_;
    std::vector<Entry> entries_;
    int32_t selectedIndex_ = -1;
    char filenameBuf_[256] = {};
    std::string errorMessage_;

    bool pendingOpen_ = false;
    bool pendingClose_ = false;
    bool pendingOverwriteConfirm_ = false;
    std::filesystem::path pendingOverwritePath_;

    static std::filesystem::path defaultStartDir();
    void navigateTo(const std::filesystem::path& dir);
    void refreshEntries();
    void drawContents();
    void drawOverwriteConfirm();
};
