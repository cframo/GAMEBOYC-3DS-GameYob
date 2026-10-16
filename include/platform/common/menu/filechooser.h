#pragma once

#include "types.h"

#include <vector>

extern bool fileChooserActive;
bool isFileChooserActive();
class FileChooser;
FileChooser* getActiveFileChooser();

class FileChooser {
public:
    FileChooser(std::string directory, std::vector<std::string> extensions, bool canQuit);

    std::string getDirectory();
    void setDirectory(std::string directory);
    char* chooseFile();

    const std::vector<std::string>& getFilenames() const { return this->filenames; }
    const std::vector<int>& getFlags() const { return this->flags; }
    int getSelection() const { return this->selection; }
    int getNumFiles() const { return this->numFiles; }
    int getScrollY() const { return this->scrollY; }
    int getFilesPerPage() const { return this->filesPerPage; }

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
    int filesPerPage = 24;
    int numFiles = 0;
    int scrollY = 0;
    std::string matchFile = "";
};
