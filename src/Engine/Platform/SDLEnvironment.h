//================ Copyright (c) 2018, PG, All rights reserved. =================//
//
// Purpose:		SDL ("partial", SDL does not provide all functions!)
//
// $NoKeywords: $sdlenv
//===============================================================================//

#ifndef SDLENVIRONMENT_H
#define SDLENVIRONMENT_H

#include "cbase.h"

#ifdef MCENGINE_FEATURE_SDL

#include "Environment.h"
#include "SDL.h"

class SDLEnvironment : public Environment {
   public:
    SDLEnvironment(SDL_Window *window);
    virtual ~SDLEnvironment() { ; }

    virtual void update();

    // engine/factory
    virtual Graphics *createRenderer();
    virtual ContextMenu *createContextMenu();

    // system
    virtual void shutdown();
    virtual void restart();
    virtual std::string getExecutablePath();
    virtual void openURLInDefaultBrowser(UString url);  // NOTE: non-SDL

    // user
    virtual UString getUsername();  // NOTE: non-SDL
    virtual std::string getUserDataPath();

    // file IO
    virtual bool fileExists(std::string filename);
    virtual bool directoryExists(std::string directoryName);  // NOTE: non-SDL
    virtual bool createDirectory(std::string directoryName);  // NOTE: non-SDL
    virtual bool renameFile(std::string oldFileName, std::string newFileName);
    virtual bool deleteFile(std::string filePath);
    virtual std::vector<UString> getFilesInFolder(UString folder);    // NOTE: non-SDL
    virtual std::vector<UString> getFoldersInFolder(UString folder);  // NOTE: non-SDL
    virtual std::vector<UString> getLogicalDrives();                  // NOTE: non-SDL
    virtual std::string getFolderFromFilePath(std::string filepath);  // NOTE: non-SDL
    virtual UString getFileExtensionFromFilePath(std::string filepath, bool includeDot = false);
    virtual std::string getFileNameFromFilePath(std::string filePath);  // NOTE: non-SDL

    // clipboard
    virtual UString getClipBoardText();
    virtual void setClipBoardText(UString text);

    // dialogs & message boxes
    virtual void showMessageInfo(UString title, UString message);
    virtual void showMessageWarning(UString title, UString message);
    virtual void showMessageError(UString title, UString message);
    virtual void showMessageErrorFatal(UString title, UString message);
    virtual UString openFileWindow(const char *filetypefilters, UString title, UString initialpath);  // NOTE: non-SDL
    virtual UString openFolderWindow(UString title, UString initialpath);                             // NOTE: non-SDL

    // window
    virtual void focus();
    virtual void center();
    virtual void minimize();
    virtual void maximize();
    virtual void enableFullscreen();
    virtual void disableFullscreen();
    virtual void setWindowTitle(UString title);
    virtual void setWindowPos(int x, int y);
    virtual void setWindowSize(int width, int height);
    virtual void setWindowResizable(bool resizable);
    virtual void setMonitor(int monitor);
    virtual Vector2 getWindowPos();
    virtual Vector2 getWindowSize();
    virtual int getMonitor();
    virtual std::vector<McRect> getMonitors() { return this->vMonitors; }
    virtual Vector2 getNativeScreenSize();
    virtual McRect getVirtualScreenRect();
    virtual McRect getDesktopRect();
    virtual int getDPI();
    virtual bool isFullscreen() { return this->bFullscreen; }
    virtual bool isWindowResizable() { return this->bResizable; }

    // mouse
    virtual bool isCursorInWindow() { return this->bIsCursorInsideWindow; }
    virtual bool isCursorVisible() { return this->bCursorVisible; }
    virtual bool isCursorClipped() { return this->bCursorClipped; }
    virtual Vector2 getMousePos();
    virtual McRect getCursorClip() { return this->cursorClip; }
    virtual CURSORTYPE getCursor() { return this->cursorType; }
    virtual void setCursor(CURSORTYPE cur);
    virtual void setCursorVisible(bool visible);
    virtual void setMousePos(int x, int y);
    virtual void setCursorClip(bool clip, McRect rect);

    // keyboard
    virtual UString keyCodeToString(KEYCODE keyCode);

    // ILLEGAL:
    void setWindow(SDL_Window *window) { m_window = window; }
    inline SDL_Window *getWindow() { return this->window; }

   protected:
    SDL_Window *m_window;

   private:
    // monitors
    std::vector<McRect> m_vMonitors;

    // window
    bool m_bResizable;
    bool m_bFullscreen;

    // mouse
    bool m_bIsCursorInsideWindow;
    bool m_bCursorVisible;
    bool m_bCursorClipped;
    McRect m_cursorClip;
    CURSORTYPE m_cursorType;

    // clipboard
    const char *m_sPrevClipboardTextSDL;
};

#endif

#endif
