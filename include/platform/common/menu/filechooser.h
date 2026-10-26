#pragma once

#include "types.h"

#include <vector>

#ifdef BACKEND_3DS
#include <3ds.h>
#else
struct touchPosition {
    u16 px;
    u16 py;
};
#endif

extern bool fileChooserActive;
bool isFileChooserActive();
class FileChooser;
FileChooser* getActiveFileChooser();

class FileChooser {
public:
    FileChooser(std::string directory, std::vector<std::string> extensions, bool canQuit);

    std::string getDirectory() const;
    std::string getSelectedRomPath() const;
    void setDirectory(std::string directory);
    char* chooseFile();

    const std::vector<std::string>& getFilenames() const { return this->filenames; }
    const std::vector<int>& getFlags() const { return this->flags; }
    int getSelection() const { return this->selection; }
    int getNumFiles() const { return this->numFiles; }
    int getScrollY() const { return (int) this->scrollY; }
    float getSubpixelScrollY() const { return this->scrollY; }
    float getMaxScroll() const;
    int getFilesPerPage() const { return this->filesPerPage; }

    void updatePhysics();
    bool navigateToSelection(char** result);

private:
    void updateScrollDown();
    void updateScrollUp();
    void navigateBack();

    void refreshContents();
    void redrawChooser();
    bool updateChooser(char** result);

    std::string directory;
    std::vector<std::string> extensions;
    bool canQuit;

    std::vector<std::string> filenames;
    std::vector<int> flags;
    int selection = 0;
    int filesPerPage = 12;
    int numFiles = 0;

    float scrollY = 0.0f;
    float targetScrollY = 0.0f;
    float scrollVelocity = 0.0f;
    bool isTouching = false;
    bool isDragging = false;
    touchPosition touchStartPos = {0, 0};
    touchPosition touchPrevPos = {0, 0};
    u64 touchStartTime = 0;

    std::string matchFile = "";
};
