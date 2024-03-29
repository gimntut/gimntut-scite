// SciTE - Scintilla based Text Editor
/** @file SciTEBuffers.cxx
 ** Buffers and jobs management.
 **/
// Copyright 1998-2010 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>

#include <string>
#include <vector>
#include <set>
#include <map>

#if defined(__unix__)

#include <unistd.h>

#if defined(GTK)
#include <gtk/gtk.h>
#endif

#else

#undef _WIN32_WINNT
#define _WIN32_WINNT  0x0500
#ifdef _MSC_VER
// windows.h, et al, use a lot of nameless struct/unions - can't fix it, so allow it
#pragma warning(disable: 4201)
#endif
#include <windows.h>
#ifdef _MSC_VER
// okay, that's done, don't allow it in our code
#pragma warning(default: 4201)
#endif
#include <commctrl.h>

// For chdir
#ifdef _MSC_VER
#include <direct.h>
#endif
#ifdef __DMC__
#include <dir.h>
#endif

#endif

#include "Scintilla.h"
#include "SciLexer.h"
#include "ILexer.h"

#include "GUI.h"

#include "SString.h"
#include "StringList.h"
#include "StringHelpers.h"
#include "FilePath.h"
#include "PropSetFile.h"
#include "StyleWriter.h"
#include "Extender.h"
#include "SciTE.h"
#include "Mutex.h"
#include "JobQueue.h"
#include "Cookie.h"
#include "Worker.h"
#include "FileWorker.h"
#include "SciTEBase.h"

const GUI::gui_char defaultSessionFileName[] = GUI_TEXT("SciTE.session");

void Buffer::CompleteLoading() {
	lifeState = open;
	if (pFileWorker && pFileWorker->IsLoading()) {
		delete pFileWorker;
		pFileWorker = 0;
	}
}

void Buffer::CompleteStoring() {
	if (pFileWorker && !pFileWorker->IsLoading()) {
		delete pFileWorker;
		pFileWorker = 0;
	}
	SetTimeFromFile();
}

void Buffer::CancelLoad() {
	// Complete any background loading
	if (pFileWorker && pFileWorker->IsLoading()) {
		pFileWorker->Cancel();
		CompleteLoading();
		lifeState = empty;
	}
}

BufferList::BufferList() : current(0), stackcurrent(0), stack(0), buffers(0), size(0), length(0), lengthVisible(0), initialised(false) {}

BufferList::~BufferList() {
	delete []buffers;
	delete []stack;
}

void BufferList::Allocate(int maxSize) {
	length = 1;
	lengthVisible = 1;
	current = 0;
	size = maxSize;
	buffers = new Buffer[size];
	stack = new int[size];
	stack[0] = 0;
}

int BufferList::Add() {
	if (length < size) {
		length++;
	}
	buffers[length - 1].Init();
	stack[length - 1] = length - 1;
	MoveToStackTop(length - 1);
//!-start-[NewBufferPosition]
	switch (SciTEBase::GetProps()->GetInt("buffers.new.position", 0)) {
	case 1:
		ShiftTo(length - 1, current + 1);
		return current + 1;
		break;
	case 2:
		ShiftTo(length - 1, 0);
		return 0;
		break;
	}
//!-end-[NewBufferPosition]
	SetVisible(length-1, true);

	return lengthVisible - 1;
}

int BufferList::GetDocumentByWorker(FileWorker *pFileWorker) const {
	for (int i = 0;i < length;i++) {
		if (buffers[i].pFileWorker == pFileWorker) {
			return i;
		}
	}
	return -1;
}

int BufferList::GetDocumentByName(FilePath filename, bool excludeCurrent) {
	if (!filename.IsSet()) {
		return -1;
	}
	for (int i = 0;i < length;i++) {
		if ((!excludeCurrent || i != current) && buffers[i].SameNameAs(filename)) {
			return i;
		}
	}
	return -1;
}

void BufferList::RemoveInvisible(int index) {
	assert(!GetVisible(index));
	if (index == current) {
		RemoveCurrent();
	} else {
		if (index < length-1) {
			// Swap with last visible
			Swap(index, length-1);
		}
		length--;
	}
}

void BufferList::RemoveCurrent() {
	// Delete and move up to fill gap but ensure doc pointer is saved.
	sptr_t currentDoc = buffers[current].doc;
	buffers[current].CompleteLoading();
	for (int i = current;i < length - 1;i++) {
		buffers[i] = buffers[i + 1];
	}
	buffers[length - 1].doc = currentDoc;

	if (length > 1) {
		CommitStackSelection();
		PopStack();
		length--;
		lengthVisible--;

		buffers[length].Init();
		if (current >= length) {
			SetCurrent(length - 1);
		}
		if (current < 0) {
			SetCurrent(0);
		}
//!-start-[ZorderSwitchingOnClose]
		if (SciTEBase::GetProps()->GetInt("buffers.zorder.switching", 0)) {
			SetCurrent(stack[stackcurrent]);
		}
//!-end-[ZorderSwitchingOnClose]
	} else {
		buffers[current].Init();
	}
	MoveToStackTop(current);
}

int BufferList::Current() const {
	return current;
}

Buffer *BufferList::CurrentBuffer() {
	return &buffers[Current()];
}

void BufferList::SetCurrent(int index) {
	current = index;
	SciTEBase::GetProps()->SetInteger("BufferNumber", current+1); //!-add-[BufferNumber]
}

void BufferList::PopStack() {
	for (int i = 0; i < length - 1; ++i) {
		int index = stack[i + 1];
		// adjust the index for items that will move in buffers[]
		if (index > current)
			--index;
		stack[i] = index;
	}
}

int BufferList::StackNext() {
	if (++stackcurrent >= length)
		stackcurrent = 0;
	return stack[stackcurrent];
}

int BufferList::StackPrev() {
	if (--stackcurrent < 0)
		stackcurrent = length - 1;
	return stack[stackcurrent];
}

void BufferList::MoveToStackTop(int index) {
	// shift top chunk of stack down into the slot that index occupies
	bool move = false;
	for (int i = length - 1; i > 0; --i) {
		if (stack[i] == index)
			move = true;
		if (move)
			stack[i] = stack[i-1];
	}
	stack[0] = index;
}

void BufferList::CommitStackSelection() {
	// called only when ctrl key is released when ctrl-tabbing
	// or when a document is closed (in case of Ctrl+F4 during ctrl-tabbing)
	MoveToStackTop(stack[stackcurrent]);
	stackcurrent = 0;
}

void BufferList::ShiftTo(int indexFrom, int indexTo) {
	// shift buffer to new place in buffers array
	if (indexFrom == indexTo ||
		indexFrom < 0 || indexFrom >= length ||
		indexTo < 0 || indexTo >= length) return;
	int step = (indexFrom > indexTo) ? -1 : 1;
	Buffer tmp = buffers[indexFrom];
	int i;
	for (i = indexFrom; i != indexTo; i += step) {
		buffers[i] = buffers[i+step];
	}
	buffers[indexTo] = tmp;
	// update stack indexes
	for (i = 0; i < length; i++) {
		if (stack[i] == indexFrom) {
			stack[i] = indexTo;
		} else if (step == 1) {
			if (indexFrom < stack[i] && stack[i] <= indexTo) stack[i] -= step;
		} else {
			if (indexFrom > stack[i] && stack[i] >= indexTo) stack[i] -= step;
		}
	}
}

void BufferList::Swap(int indexA, int indexB) {
	// shift buffer to new place in buffers array
	if (indexA == indexB ||
		indexA < 0 || indexA >= length ||
		indexB < 0 || indexB >= length) return;
	Buffer tmp = buffers[indexA];
	buffers[indexA] = buffers[indexB];
	buffers[indexB] = tmp;
	// update stack indexes
	for (int i = 0; i < length; i++) {
		if (stack[i] == indexA) {
			stack[i] = indexB;
		} else if (stack[i] == indexB) {
			stack[i] = indexA;
		}
	}
}

BackgroundActivities BufferList::CountBackgroundActivities() const {
	BackgroundActivities bg;
	bg.loaders = 0;
	bg.storers = 0;
	bg.totalWork = 0;
	bg.totalProgress = 0;
	for (int i = 0;i < length;i++) {
		if (buffers[i].pFileWorker) {
			if (!buffers[i].pFileWorker->FinishedJob()) {
				if (buffers[i].pFileWorker->IsLoading())
					bg.loaders++;
				else
					bg.storers++;
				bg.fileNameLast = buffers[i].AsInternal();
				bg.totalWork += buffers[i].pFileWorker->jobSize;
				bg.totalProgress += buffers[i].pFileWorker->jobProgress;
			}
		}
	}
	return bg;
}

bool BufferList::SavingInBackground() const {
	for (int i = 0; i<length; i++) {
		if (buffers[i].pFileWorker && !buffers[i].pFileWorker->IsLoading() && !buffers[i].pFileWorker->FinishedJob()) {
			return true;
		}
	}
	return false;
}

bool BufferList::GetVisible(int index) {
	return index < lengthVisible;
}

void BufferList::SetVisible(int index, bool visible) {
	if (visible != GetVisible(index)) {
		if (visible) {
			if (index > lengthVisible) {
				// Swap with first invisible
				Swap(index, lengthVisible);
			}
			lengthVisible++;
		} else {
			if (index < lengthVisible-1) {
				// Swap with last visible
				Swap(index, lengthVisible-1);
			}
			lengthVisible--;
			if (current >= lengthVisible && lengthVisible > 0)
				SetCurrent(lengthVisible-1);
		}
	}
}

void BufferList::AddFuture(int index, Buffer::FutureDo fd) {
	if (index >= 0 || index < length) {
		buffers[index].futureDo = static_cast<Buffer::FutureDo>(buffers[index].futureDo | fd);
	}
}

void BufferList::FinishedFuture(int index, Buffer::FutureDo fd) {
	if (index >= 0 || index < length) {
		buffers[index].futureDo = static_cast<Buffer::FutureDo>(buffers[index].futureDo & ~(fd));
	}
}

sptr_t SciTEBase::GetDocumentAt(int index) {
	if (index < 0 || index >= buffers.size) {
		return 0;
	}
	if (buffers.buffers[index].doc == 0) {
		// Create a new document buffer
		buffers.buffers[index].doc = wEditor.CallReturnPointer(SCI_CREATEDOCUMENT, 0, 0);
	}
	return buffers.buffers[index].doc;
}

void SciTEBase::SwitchDocumentAt(int index, sptr_t pdoc) {
	if (index < 0 || index >= buffers.size) {
		return;
	}
	sptr_t pdocOld = buffers.buffers[index].doc;
	buffers.buffers[index].doc = pdoc;
	if (pdocOld) {
		wEditor.Call(SCI_RELEASEDOCUMENT, 0, pdocOld);
	}
	if (index == buffers.Current()) {
		wEditor.Call(SCI_SETDOCPOINTER, 0, buffers.buffers[index].doc);
	}
}

void SciTEBase::SetDocumentAt(int index, bool updateStack) {
	int currentbuf = buffers.Current();

	if (	index < 0 ||
	        index >= buffers.length ||
	        index == currentbuf ||
	        currentbuf < 0 ||
	        currentbuf >= buffers.length) {
		return;
	}
	UpdateBuffersCurrent();

	buffers.SetCurrent(index);
	if (updateStack) {
		buffers.MoveToStackTop(index);
	}

	if (extender) {
		if (buffers.size > 1)
			extender->ActivateBuffer(index);
		else
			extender->InitBuffer(0);
	}

	Buffer bufferNext = buffers.buffers[buffers.Current()];
	SetFileName(bufferNext);
	propsDiscovered = bufferNext.props;
	propsDiscovered.superPS = &propsLocal;
	wEditor.Call(SCI_SETDOCPOINTER, 0, GetDocumentAt(buffers.Current()));
	bool restoreBookmarks = bufferNext.lifeState == Buffer::readAll;
	PerformDeferredTasks();
	if (bufferNext.lifeState == Buffer::readAll) {
		CompleteOpen(ocCompleteSwitch);
		if (extender)
			extender->OnOpen(filePath.AsUTF8().c_str());
	}
	RestoreState(bufferNext, restoreBookmarks);

	TabSelect(index);

	if (lineNumbers && lineNumbersExpand)
		SetLineNumberWidth();

	DisplayAround(bufferNext);
	if (restoreBookmarks) {
		// Restoring a session does not restore the scroll position
		// so make the selection visible.
		wEditor.Call(SCI_SCROLLCARET);
	}

	SetBuffersMenu();
	CheckMenus();
	UpdateStatusBar(true);

	if (extender) {
		extender->OnSwitchFile(filePath.AsUTF8().c_str());
	}
}

void SciTEBase::UpdateBuffersCurrent() {
	int currentbuf = buffers.Current();

	if ((buffers.length > 0) && (currentbuf >= 0) && (buffers.GetVisible(currentbuf))) {
		Buffer &bufferCurrent = buffers.buffers[currentbuf];
		bufferCurrent.Set(filePath);
		if (bufferCurrent.lifeState != Buffer::reading && bufferCurrent.lifeState != Buffer::readAll) {
			bufferCurrent.selection.position = wEditor.Call(SCI_GETCURRENTPOS);
			bufferCurrent.selection.anchor = wEditor.Call(SCI_GETANCHOR);
			bufferCurrent.scrollPosition = GetCurrentScrollPosition();

			// Retrieve fold state and store in buffer state info

			std::vector<int> *f = &bufferCurrent.foldState;
			f->clear();

			if (props.GetInt("fold")) {
				for (int line = 0; ; line++) {
					int lineNext = wEditor.Call(SCI_CONTRACTEDFOLDNEXT, line);
					if ((line < 0) || (lineNext < line))
						break;
					line = lineNext;
					f->push_back(line);
				}
			}

			if (props.GetInt("session.bookmarks")) {
				buffers.buffers[buffers.Current()].bookmarks.clear();
				int lineBookmark = -1;
				while ((lineBookmark = wEditor.Call(SCI_MARKERNEXT, lineBookmark + 1, 1 << markerBookmark)) >= 0) {
					bufferCurrent.bookmarks.push_back(lineBookmark);
				}
			}
		}
	}
}

bool SciTEBase::IsBufferAvailable() {
	return buffers.size > 1 && buffers.length < buffers.size;
}

bool SciTEBase::CanMakeRoom(bool maySaveIfDirty) {
	if (IsBufferAvailable()) {
		return true;
	} else if (maySaveIfDirty) {
		// All available buffers are taken, try and close the current one
		if (SaveIfUnsure(true) != IDCANCEL) {
			// The file isn't dirty, or the user agreed to close the current one
			return true;
		}
	} else {
		return true;	// Told not to save so must be OK.
	}
	return false;
}

void SciTEBase::ClearDocument() {
	wEditor.Call(SCI_SETREADONLY, 0);
	wEditor.Call(SCI_SETUNDOCOLLECTION, 0);
	wEditor.Call(SCI_CLEARALL);
	wEditor.Call(SCI_EMPTYUNDOBUFFER);
	wEditor.Call(SCI_SETUNDOCOLLECTION, 1);
	wEditor.Call(SCI_SETSAVEPOINT);
	wEditor.Call(SCI_SETREADONLY, isReadOnly);
}

void SciTEBase::CreateBuffers() {
	int buffersWanted = props.GetInt("buffers");
	if (buffersWanted > bufferMax) {
		buffersWanted = bufferMax;
	}
	if (buffersWanted < 1) {
		buffersWanted = 1;
	}
	buffers.Allocate(buffersWanted);
}

void SciTEBase::InitialiseBuffers() {
	if (!buffers.initialised) {
		buffers.initialised = true;
		// First document is the default from creation of control
		buffers.buffers[0].doc = wEditor.CallReturnPointer(SCI_GETDOCPOINTER, 0, 0);
		wEditor.Call(SCI_ADDREFDOCUMENT, 0, buffers.buffers[0].doc); // We own this reference
		if (buffers.size == 1) {
			// Single buffer mode, delete the Buffers main menu entry
			DestroyMenuItem(menuBuffers, 0);
			// Destroy command "View Tab Bar" in the menu "View"
			DestroyMenuItem(menuView, IDM_VIEWTABBAR);
			// Make previous change visible.
			RedrawMenu();
		}
	}
}

FilePath SciTEBase::UserFilePath(const GUI::gui_char *name) {
	GUI::gui_string nameWithVisibility(configFileVisibilityString);
	nameWithVisibility += name;
	return FilePath(GetSciteUserHome(), nameWithVisibility.c_str());
}

static SString IndexPropKey(const char *bufPrefix, int bufIndex, const char *bufAppendix) {
	SString pKey = bufPrefix;
	pKey += '.';
	pKey += SString(bufIndex + 1);
	if (bufAppendix != NULL) {
		pKey += ".";
		pKey += bufAppendix;
	}
	return pKey;
}

void SciTEBase::LoadSessionFile(const GUI::gui_char *sessionName) {
	FilePath sessionPathName;
	if (sessionName[0] == '\0') {
		sessionPathName = UserFilePath(defaultSessionFileName);
	} else {
		sessionPathName.Set(sessionName);
	}

	propsSession.Clear();
	propsSession.Read(sessionPathName, sessionPathName.Directory(), filter, NULL);

	FilePath sessionFilePath = FilePath(sessionPathName).AbsolutePath();
	// Add/update SessionPath environment variable
	props.Set("SessionPath", sessionFilePath.AsUTF8().c_str());
}

void SciTEBase::RestoreRecentMenu() {
	SelectedRange sr(0,0);

	DeleteFileStackMenu();

	for (int i = 0; i < fileStackMax; i++) {
		SString propKey = IndexPropKey("mru", i, "path");
		SString propStr = propsSession.Get(propKey.c_str());
		if (propStr == "")
			continue;
		AddFileToStack(GUI::StringFromUTF8(propStr.c_str()), sr, 0);
	}
}

static std::vector<int> LinesFromString(const SString &s) {
	std::vector<int> result;
	if (s.length()) {
		char *buf = new char[s.length() + 1];
		strcpy(buf, s.c_str());
		char *p = strtok(buf, ",");
		while (p != NULL) {
			int line = atoi(p) - 1;
			result.push_back(line);
			p = strtok(NULL, ",");
		}
		delete []buf;
	}
	return result;
}

void SciTEBase::RestoreFromSession(const Session &session) {
	for (std::vector<BufferState>::const_iterator bs=session.buffers.begin(); bs != session.buffers.end(); ++bs)
		AddFileToBuffer(*bs);
	int iBuffer = buffers.GetDocumentByName(session.pathActive);
	if (iBuffer >= 0)
		SetDocumentAt(iBuffer);
}

void SciTEBase::RestoreSession() {
	if (props.GetInt("session.close.buffers.onload", 1) == 1) //!-add-[session.close.buffers.onload]
	// Comment next line if you don't want to close all buffers before restoring session
	CloseAllBuffers(true);

	Session session;

	for (int i = 0; i < bufferMax; i++) {
		SString propKey = IndexPropKey("buffer", i, "path");
		SString propStr = propsSession.Get(propKey.c_str());
		if (propStr == "")
			continue;

		BufferState bufferState;
		bufferState.Set(GUI::StringFromUTF8(propStr.c_str()));

		propKey = IndexPropKey("buffer", i, "current");
		if (propsSession.GetInt(propKey.c_str()))
			session.pathActive = bufferState;

		propKey = IndexPropKey("buffer", i, "position");
		int pos = propsSession.GetInt(propKey.c_str());

		bufferState.selection.anchor = pos - 1;
		bufferState.selection.position = bufferState.selection.anchor;

		if (props.GetInt("session.bookmarks")) {
			propKey = IndexPropKey("buffer", i, "bookmarks");
			propStr = propsSession.Get(propKey.c_str());
			bufferState.bookmarks = LinesFromString(propStr);
		}

		if (props.GetInt("fold") && !props.GetInt("fold.on.open") &&
			props.GetInt("session.folds")) {
			propKey = IndexPropKey("buffer", i, "folds");
			propStr = propsSession.Get(propKey.c_str());
			bufferState.foldState = LinesFromString(propStr);
		}

		session.buffers.push_back(bufferState);
	}

	RestoreFromSession(session);
}

void SciTEBase::SaveSessionFile(const GUI::gui_char *sessionName) {
	UpdateBuffersCurrent();
	bool defaultSession;
	FilePath sessionPathName;
	if (sessionName[0] == '\0') {
		sessionPathName = UserFilePath(defaultSessionFileName);
		defaultSession = true;
	} else {
		sessionPathName.Set(sessionName);
		defaultSession = false;
	}
	FILE *sessionFile = sessionPathName.Open(fileWrite);
	if (!sessionFile)
		return;

	fprintf(sessionFile, "# SciTE session file\n");

	if (defaultSession && props.GetInt("save.position")) {
		int top, left, width, height, maximize;
		GetWindowPosition(&left, &top, &width, &height, &maximize);

		fprintf(sessionFile, "\n");
		fprintf(sessionFile, "position.left=%d\n", left);
		fprintf(sessionFile, "position.top=%d\n", top);
		fprintf(sessionFile, "position.width=%d\n", width);
		fprintf(sessionFile, "position.height=%d\n", height);
		fprintf(sessionFile, "position.maximize=%d\n", maximize);
	}

	if (defaultSession && props.GetInt("save.recent")) {
		SString propKey;
		int j = 0;

		fprintf(sessionFile, "\n");

		// Save recent files list
		for (int i = fileStackMax - 1; i >= 0; i--) {
			if (recentFileStack[i].IsSet()) {
				propKey = IndexPropKey("mru", j++, "path");
				fprintf(sessionFile, "%s=%s\n", propKey.c_str(), recentFileStack[i].AsUTF8().c_str());
			}
		}
	}

	if (props.GetInt("buffers") && (!defaultSession || props.GetInt("save.session"))) {
		int curr = buffers.Current();
		for (int i = 0; i < buffers.lengthVisible; i++) {
			if (buffers.buffers[i].IsSet() && !buffers.buffers[i].IsUntitled()) {
				Buffer &buff = buffers.buffers[i];
				SString propKey = IndexPropKey("buffer", i, "path");
				fprintf(sessionFile, "\n%s=%s\n", propKey.c_str(), buff.AsUTF8().c_str());

				int pos = buff.selection.position + 1;
				propKey = IndexPropKey("buffer", i, "position");
				fprintf(sessionFile, "%s=%d\n", propKey.c_str(), pos);

				if (i == curr) {
					propKey = IndexPropKey("buffer", i, "current");
					fprintf(sessionFile, "%s=1\n", propKey.c_str());
				}

				if (props.GetInt("session.bookmarks")) {
					bool found = false;
					for (std::vector<int>::iterator itBM=buff.bookmarks.begin();
						itBM != buff.bookmarks.end(); ++itBM) {
						if (!found) {
							propKey = IndexPropKey("buffer", i, "bookmarks");
							fprintf(sessionFile, "%s=%d", propKey.c_str(), *itBM + 1);
							found = true;
						} else {
							fprintf(sessionFile, ",%d", *itBM + 1);
						}
					}
					if (found)
						fprintf(sessionFile, "\n");
				}

				if (props.GetInt("fold") && props.GetInt("session.folds")) {
					bool found = false;
					for (std::vector<int>::iterator itF=buff.foldState.begin();
						itF != buff.foldState.end(); ++itF) {
						if (!found) {
							propKey = IndexPropKey("buffer", i, "folds");
							fprintf(sessionFile, "%s=%d", propKey.c_str(), *itF + 1);
							found = true;
						} else {
							fprintf(sessionFile, ",%d", *itF + 1);
						}
					}
					if (found)
						fprintf(sessionFile, "\n");
				}
			}
		}
	}

	fclose(sessionFile);

	FilePath sessionFilePath = FilePath(sessionPathName).AbsolutePath();
	// Add/update SessionPath environment variable
	props.Set("SessionPath", sessionFilePath.AsUTF8().c_str());
}

void SciTEBase::SetIndentSettings() {
	// Get default values
	int useTabs = props.GetInt("use.tabs", 1);
	int tabSize = props.GetInt("tabsize");
	int indentSize = props.GetInt("indent.size");
	// Either set the settings related to the extension or the default ones
	SString fileNameForExtension = ExtensionFileName();
	SString useTabsChars = props.GetNewExpand("use.tabs.",
	        fileNameForExtension.c_str());
	if (useTabsChars.length() != 0) {
		wEditor.Call(SCI_SETUSETABS, useTabsChars.value());
	} else {
		wEditor.Call(SCI_SETUSETABS, useTabs);
	}
	SString tabSizeForExt = props.GetNewExpand("tab.size.",
	        fileNameForExtension.c_str());
	if (tabSizeForExt.length() != 0) {
		wEditor.Call(SCI_SETTABWIDTH, tabSizeForExt.value());
	} else if (tabSize != 0) {
		wEditor.Call(SCI_SETTABWIDTH, tabSize);
	}
	SString indentSizeForExt = props.GetNewExpand("indent.size.",
	        fileNameForExtension.c_str());
	if (indentSizeForExt.length() != 0) {
		wEditor.Call(SCI_SETINDENT, indentSizeForExt.value());
	} else {
		wEditor.Call(SCI_SETINDENT, indentSize);
	}
}

void SciTEBase::SetEol() {
	SString eol_mode = props.Get("eol.mode");
	if (eol_mode == "LF") {
		wEditor.Call(SCI_SETEOLMODE, SC_EOL_LF);
	} else if (eol_mode == "CR") {
		wEditor.Call(SCI_SETEOLMODE, SC_EOL_CR);
	} else if (eol_mode == "CRLF") {
		wEditor.Call(SCI_SETEOLMODE, SC_EOL_CRLF);
	}
}

void SciTEBase::New() {
	InitialiseBuffers();
	UpdateBuffersCurrent();

	if ((buffers.size == 1) && (!buffers.buffers[0].IsUntitled())) {
		AddFileToStack(buffers.buffers[0],
		        buffers.buffers[0].selection,
		        buffers.buffers[0].scrollPosition);
	}

	// If the current buffer is the initial untitled, clean buffer then overwrite it,
	// otherwise add a new buffer.
	if ((buffers.length > 1) ||
	        (buffers.Current() != 0) ||
	        (buffers.buffers[0].isDirty) ||
	        (!buffers.buffers[0].IsUntitled())) {
		if (buffers.size == buffers.length) {
			Close(false, false, true);
		}
		buffers.SetCurrent(buffers.Add());
	}

	sptr_t doc = GetDocumentAt(buffers.Current());
	wEditor.Call(SCI_SETDOCPOINTER, 0, doc);

	FilePath curDirectory(filePath.Directory());
	filePath.Set(curDirectory, GUI_TEXT(""));
	SetFileName(filePath);
	UpdateBuffersCurrent();
	SetBuffersMenu();
	CurrentBuffer()->isDirty = false;
	CurrentBuffer()->lifeState = Buffer::open;
	jobQueue.isBuilding = false;
	jobQueue.isBuilt = false;
	isReadOnly = false;	// No sense to create an empty, read-only buffer...

	ClearDocument();
	DeleteFileStackMenu();
	SetFileStackMenu();
	if (extender)
		extender->InitBuffer(buffers.Current());
}

void SciTEBase::RestoreState(const Buffer &buffer, bool restoreBookmarks) {
	SetWindowName();
	ReadProperties();
	if (CurrentBuffer()->unicodeMode != uni8Bit) {
		// Override the code page if Unicode
		codePage = SC_CP_UTF8;
		wEditor.Call(SCI_SETCODEPAGE, codePage);
	}
	props.SetInteger("editor.unicode.mode", CurrentBuffer()->unicodeMode + IDM_ENCODING_DEFAULT); //!-add-[EditorUnicodeMode]
	isReadOnly = wEditor.Call(SCI_GETREADONLY);

	// check to see whether there is saved fold state, restore
	if (!buffer.foldState.empty()) {
		wEditor.Call(SCI_COLOURISE, 0, -1);
		for (std::vector<int>::const_iterator fold=buffer.foldState.begin(); fold != buffer.foldState.end(); ++fold) {
			wEditor.Call(SCI_TOGGLEFOLD, *fold);
		}
	}
	if (restoreBookmarks) {
		for (std::vector<int>::const_iterator mark=buffer.bookmarks.begin(); mark != buffer.bookmarks.end(); ++mark) {
			wEditor.Call(SCI_MARKERADD, *mark, markerBookmark);
		}
	}
}

void SciTEBase::Close(bool updateUI, bool loadingSession, bool makingRoomForNew) {
	bool closingLast = false;
	int index = buffers.Current();
	if (index >= 0) {
		buffers.buffers[index].CancelLoad();
	}

	if (extender) {
		extender->OnClose(filePath.AsUTF8().c_str());
	}

	if (buffers.size == 1) {
		// With no buffer list, Close means close from MRU
		closingLast = !(recentFileStack[0].IsSet());
		buffers.buffers[0].Init();
		filePath.Set(GUI_TEXT(""));
		ClearDocument(); //avoid double are-you-sure
		if (!makingRoomForNew)
			StackMenu(0); // calls New, or Open, which calls InitBuffer
	} else if (buffers.size > 1) {
		if (buffers.Current() >= 0 && buffers.Current() < buffers.length) {
			UpdateBuffersCurrent();
			Buffer buff = buffers.buffers[buffers.Current()];
			AddFileToStack(buff, buff.selection, buff.scrollPosition);
		}
		closingLast = (buffers.lengthVisible == 1) && !buffers.buffers[0].pFileWorker;
		if (closingLast) {
			buffers.buffers[0].Init();
			buffers.buffers[0].lifeState = Buffer::open;
			if (extender)
				extender->InitBuffer(0);
		} else {
			if (extender)
				extender->RemoveBuffer(buffers.Current());
			if (buffers.buffers[buffers.Current()].pFileWorker) {
				buffers.SetVisible(buffers.Current(), false);
				if (buffers.lengthVisible == 0)
					New();
			} else {
				wEditor.Call(SCI_SETREADONLY, 0);
				ClearDocument();
				buffers.RemoveCurrent();
			}
			if (extender && !makingRoomForNew)
				extender->ActivateBuffer(buffers.Current());
		}
		Buffer bufferNext = buffers.buffers[buffers.Current()];

		if (updateUI)
			SetFileName(bufferNext);
		else
			filePath = bufferNext;
		propsDiscovered = bufferNext.props;
		propsDiscovered.superPS = &propsLocal;
		wEditor.Call(SCI_SETDOCPOINTER, 0, GetDocumentAt(buffers.Current()));
		PerformDeferredTasks();
		if (bufferNext.lifeState == Buffer::readAll) {
			//restoreBookmarks = true;
			CompleteOpen(ocCompleteSwitch);
			if (extender)
				extender->OnOpen(filePath.AsUTF8().c_str());
		}
		if (closingLast) {
			wEditor.Call(SCI_SETREADONLY, 0);
			ClearDocument();
		}
		if (updateUI)
			CheckReload();
		if (updateUI) {
			RestoreState(bufferNext, false);
			DisplayAround(bufferNext);
		}
	}

	if (updateUI) {
		BuffersMenu();
		UpdateStatusBar(true);
	}

	if (extender && !closingLast && !makingRoomForNew) {
		extender->OnSwitchFile(filePath.AsUTF8().c_str());
	}

	if (closingLast && props.GetInt("quit.on.close.last") && !loadingSession) {
		QuitProgram();
	}
}

void SciTEBase::CloseTab(int tab) {
	int tabCurrent = buffers.Current();
	if (tab == tabCurrent) {
		if (SaveIfUnsure() != IDCANCEL) {
			Close();
		}
	} else {
		FilePath fpCurrent = buffers.buffers[tabCurrent].AbsolutePath();
		SetDocumentAt(tab);
		if (SaveIfUnsure() != IDCANCEL) {
			Close();
			WindowSetFocus(wEditor);
			// Return to the previous buffer
			SetDocumentAt(buffers.GetDocumentByName(fpCurrent));
		}
	}
}

void SciTEBase::CloseAllBuffers(bool loadingSession) {
	if (SaveAllBuffers(false) != IDCANCEL) {
		while (buffers.lengthVisible > 1)
			Close(false, loadingSession);

		Close(true, loadingSession);
	}
}

int SciTEBase::SaveAllBuffers(bool forceQuestion, bool alwaysYes) {
	int choice = IDYES;
	UpdateBuffersCurrent();
	int currentBuffer = buffers.Current();
	for (int i = 0; (i < buffers.lengthVisible) && (choice != IDCANCEL); i++) {
//!		if (buffers.buffers[i].isDirty) {
		if (buffers.buffers[i].DocumentNotSaved()) { //!-change-[OpenNonExistent]
			SetDocumentAt(i);
			if (alwaysYes) {
				if (!Save()) {
					choice = IDCANCEL;
				}
			} else {
				choice = SaveIfUnsure(forceQuestion);
			}
		}
	}
	SetDocumentAt(currentBuffer);
	return choice;
}

void SciTEBase::SaveTitledBuffers() {
	UpdateBuffersCurrent();
	int currentBuffer = buffers.Current();
	for (int i = 0; i < buffers.lengthVisible; i++) {
//!		if (buffers.buffers[i].isDirty && !buffers.buffers[i].IsUntitled()) {
		if (buffers.buffers[i].DocumentNotSaved() && !buffers.buffers[i].IsUntitled()) { //!-change-[OpenNonExistent]
			SetDocumentAt(i);
			Save();
		}
	}
	SetDocumentAt(currentBuffer);
}

void SciTEBase::Next() {
	int next = buffers.Current();
	if (++next >= buffers.lengthVisible)
		next = 0;
	SetDocumentAt(next);
	CheckReload();
}

void SciTEBase::Prev() {
	int prev = buffers.Current();
	if (--prev < 0)
		prev = buffers.lengthVisible - 1;

	SetDocumentAt(prev);
	CheckReload();
}

void SciTEBase::ShiftTab(int indexFrom, int indexTo) {
	buffers.ShiftTo(indexFrom, indexTo);
	buffers.SetCurrent(indexTo);
	BuffersMenu();

	TabSelect(indexTo);

	DisplayAround(buffers.buffers[buffers.Current()]);
}

void SciTEBase::MoveTabRight() {
	if (buffers.lengthVisible < 2) return;
	int indexFrom = buffers.Current();
	int indexTo = indexFrom + 1;
	if (indexTo >= buffers.lengthVisible) indexTo = 0;
	ShiftTab(indexFrom, indexTo);
}

void SciTEBase::MoveTabLeft() {
	if (buffers.lengthVisible < 2) return;
	int indexFrom = buffers.Current();
	int indexTo = indexFrom - 1;
	if (indexTo < 0) indexTo = buffers.lengthVisible - 1;
	ShiftTab(indexFrom, indexTo);
}

void SciTEBase::NextInStack() {
	SetDocumentAt(buffers.StackNext(), false);
	CheckReload();
}

void SciTEBase::PrevInStack() {
	SetDocumentAt(buffers.StackPrev(), false);
	CheckReload();
}

void SciTEBase::EndStackedTabbing() {
	buffers.CommitStackSelection();
}

static void EscapeFilePathsForMenu(GUI::gui_string &path) {
	// Escape '&' characters in path, since they are interpreted in
	// menues.
	Substitute(path, GUI_TEXT("&"), GUI_TEXT("&&"));
#if defined(GTK)
	GUI::gui_string homeDirectory = getenv("HOME");
	if (StartsWith(path, homeDirectory)) {
		path.replace(static_cast<size_t>(0), homeDirectory.size(), GUI_TEXT("~"));
	}
#endif
}

void SciTEBase::SetBuffersMenu() {
	if (buffers.size <= 1) {
        DestroyMenuItem(menuBuffers, IDM_BUFFERSEP);
    }
	RemoveAllTabs();

	int pos;
	for (pos = buffers.lengthVisible; pos < bufferMax; pos++) {
		DestroyMenuItem(menuBuffers, IDM_BUFFER + pos);
	}
	if (buffers.size > 1) {
//!		int menuStart = 4;
		int menuStart = 7; //!-change-[TabsMoving]
		unsigned tabsTitleMaxLength = props.GetInt("tabbar.title.maxlength",0); //!-add-[TabbarTitleMaxLength]
		SetMenuItem(menuBuffers, menuStart, IDM_BUFFERSEP, GUI_TEXT(""));
		for (pos = 0; pos < buffers.lengthVisible; pos++) {
			int itemID = bufferCmdID + pos;
			GUI::gui_string entry;
			GUI::gui_string titleTab;

			if (pos < 10) {
				GUI::gui_string sPos = GUI::StringFromInteger((pos + 1) % 10);
				GUI::gui_string sHotKey = GUI_TEXT("&") + sPos + GUI_TEXT(" ");
#if defined(WIN32)
				entry = sHotKey;	// hotkey 1..0
				titleTab = sHotKey; // add hotkey to the tabbar
#elif defined(GTK)
				entry = sHotKey;	// hotkey 1..0
				titleTab = sPos + GUI_TEXT(" ");
#endif
			}

			if (buffers.buffers[pos].IsUntitled()) {
				GUI::gui_string untitled = localiser.Text("Untitled");
				entry += untitled;
				titleTab += untitled;
			} else {
				GUI::gui_string path = buffers.buffers[pos].AsInternal();
				GUI::gui_string filename = buffers.buffers[pos].Name().AsInternal();

				EscapeFilePathsForMenu(path);
#if defined(WIN32)
				// On Windows, '&' are also interpreted in tab names, so we need
				// the escaped filename
				EscapeFilePathsForMenu(filename);
#endif
				entry += path;
//!-start-[TabbarTitleMaxLength]
				if (tabsTitleMaxLength > 0 && titleTab.length() > tabsTitleMaxLength + 3) {
					titleTab.resize(tabsTitleMaxLength, L'\0');
					titleTab.append(GUI_TEXT("..."));
				}
//!-end-[TabbarTitleMaxLength]
				titleTab += filename;
			}
			// For short file names:
			//char *cpDirEnd = strrchr(buffers.buffers[pos]->fileName, pathSepChar);
			//strcat(entry, cpDirEnd + 1);

//!			if (buffers.buffers[pos].isDirty) {
			if (buffers.buffers[pos].DocumentNotSaved()) { //!-change-[OpenNonExistent]
				entry += GUI_TEXT(" *");
				titleTab += GUI_TEXT(" *");
			}
//!-start-[ReadOnlyTabMarker]
			if (buffers.buffers[pos].ROMarker != NULL) {
				entry += buffers.buffers[pos].ROMarker;
				titleTab += buffers.buffers[pos].ROMarker;
			}
//!-end-[ReadOnlyTabMarker]

			SetMenuItem(menuBuffers, menuStart + pos + 1, itemID, entry.c_str());
			TabInsert(pos, titleTab.c_str());
		}
	}
	CheckMenus();
#if !defined(GTK)

	if (tabVisible)
		SizeSubWindows();
#endif
#if defined(GTK)
	ShowTabBar();
#endif
}

void SciTEBase::BuffersMenu() {
	UpdateBuffersCurrent();
	SetBuffersMenu();
}

void SciTEBase::DeleteFileStackMenu() {
	for (int stackPos = 0; stackPos < fileStackMax; stackPos++) {
		DestroyMenuItem(menuFile, fileStackCmdID + stackPos);
	}
	DestroyMenuItem(menuFile, IDM_MRU_SEP);
}

void SciTEBase::SetFileStackMenu() {
	if (recentFileStack[0].IsSet()) {
		SetMenuItem(menuFile, MRU_START, IDM_MRU_SEP, GUI_TEXT(""));
//!		for (int stackPos = 0; stackPos < fileStackMax; stackPos++) {
//!-start-[MoreRecentFiles]
		int fileStackMaxToUse = props.GetInt("save.recent.max",fileStackMaxDefault); //-> props
		if ( fileStackMaxToUse > fileStackMax )
			 fileStackMaxToUse = fileStackMax;
		for (int stackPos = 0; stackPos < fileStackMaxToUse; stackPos++) {
//!-end-[MoreRecentFiles]
			int itemID = fileStackCmdID + stackPos;
			if (recentFileStack[stackPos].IsSet()) {
				GUI::gui_char entry[MAX_PATH + 20];
				entry[0] = '\0';
				if ( stackPos < 10 ) //!-add-[MoreRecentFiles]
#if defined(GTK)
				sprintf(entry, GUI_TEXT("&%d "), (stackPos + 1) % 10);
#elif defined(WIN32)
#if defined(_MSC_VER) && (_MSC_VER > 1200)
				swprintf(entry, ELEMENTS(entry), GUI_TEXT("&%d "), (stackPos + 1) % 10);
#else
				swprintf(entry, GUI_TEXT("&%d "), (stackPos + 1) % 10);
				else swprintf(entry, GUI_TEXT("   ")); //!-add-[MoreRecentFiles]
#endif
#endif
				GUI::gui_string path = recentFileStack[stackPos].AsInternal();
				EscapeFilePathsForMenu(path);

				GUI::gui_string sEntry(entry);
				sEntry += path;
				SetMenuItem(menuFile, MRU_START + stackPos + 1, itemID, sEntry.c_str());
			}
		}
	}
}

void SciTEBase::DropFileStackTop() {
	DeleteFileStackMenu();
	for (int stackPos = 0; stackPos < fileStackMax - 1; stackPos++)
		recentFileStack[stackPos] = recentFileStack[stackPos + 1];
	recentFileStack[fileStackMax - 1].Init();
	SetFileStackMenu();
}

bool SciTEBase::AddFileToBuffer(const BufferState &bufferState) {
	// Return whether file loads successfully
	bool opened = false;
	if (bufferState.Exists()) {
		opened = Open(bufferState, static_cast<OpenFlags>(ofForceLoad));
		// If forced synchronous should set up position, foldState and bookmarks
		if (opened) {
			int iBuffer = buffers.GetDocumentByName(bufferState, false);
			if (iBuffer >= 0) {
				buffers.buffers[iBuffer].scrollPosition = 0;
				buffers.buffers[iBuffer].selection = bufferState.selection;
				buffers.buffers[iBuffer].foldState = bufferState.foldState;
				buffers.buffers[iBuffer].bookmarks = bufferState.bookmarks;
			}
		}
	}
	return opened;
}

void SciTEBase::AddFileToStack(FilePath file, SelectedRange selection, int scrollPos) {
	if (!file.IsSet())
		return;
	DeleteFileStackMenu();
	// Only stack non-empty names
	if (file.IsSet() && !file.IsUntitled()) {
		int stackPos;
		int eqPos = fileStackMax - 1;
		for (stackPos = 0; stackPos < fileStackMax; stackPos++)
			if (recentFileStack[stackPos].SameNameAs(file))
				eqPos = stackPos;
		for (stackPos = eqPos; stackPos > 0; stackPos--)
			recentFileStack[stackPos] = recentFileStack[stackPos - 1];
		recentFileStack[0].Set(file);
		recentFileStack[0].selection = selection;
		recentFileStack[0].scrollPosition = scrollPos;
	}
	SetFileStackMenu();
}

void SciTEBase::RemoveFileFromStack(FilePath file) {
	if (!file.IsSet())
		return;
	DeleteFileStackMenu();
	int stackPos;
	for (stackPos = 0; stackPos < fileStackMax; stackPos++) {
		if (recentFileStack[stackPos].SameNameAs(file)) {
			for (int movePos = stackPos; movePos < fileStackMax - 1; movePos++)
				recentFileStack[movePos] = recentFileStack[movePos + 1];
			recentFileStack[fileStackMax - 1].Init();
			break;
		}
	}
	SetFileStackMenu();
}

RecentFile SciTEBase::GetFilePosition() {
	RecentFile rf;
	rf.selection = GetSelectedRange();
	rf.scrollPosition = GetCurrentScrollPosition();
	return rf;
}

void SciTEBase::DisplayAround(const RecentFile &rf) {
	if ((rf.selection.position != INVALID_POSITION) && (rf.selection.anchor != INVALID_POSITION)) {
		SetSelection(rf.selection.anchor, rf.selection.position);

		int curTop = wEditor.Call(SCI_GETFIRSTVISIBLELINE);
		int lineTop = wEditor.Call(SCI_VISIBLEFROMDOCLINE, rf.scrollPosition);
		wEditor.Call(SCI_LINESCROLL, 0, lineTop - curTop);
	}
}

// Next and Prev file comments.
// Decided that "Prev" file should mean the file you had opened last
// This means "Next" file means the file you opened longest ago.
void SciTEBase::StackMenuNext() {
	DeleteFileStackMenu();
	for (int stackPos = fileStackMax - 1; stackPos >= 0;stackPos--) {
		if (recentFileStack[stackPos].IsSet()) {
			SetFileStackMenu();
			StackMenu(stackPos);
			return;
		}
	}
	SetFileStackMenu();
}

void SciTEBase::StackMenuPrev() {
	if (recentFileStack[0].IsSet()) {
		// May need to restore last entry if removed by StackMenu
		RecentFile rfLast = recentFileStack[fileStackMax - 1];
		StackMenu(0);	// Swap current with top of stack
		for (int checkPos = 0; checkPos < fileStackMax; checkPos++) {
			if (rfLast.SameNameAs(recentFileStack[checkPos])) {
				rfLast.Init();
			}
		}
		// And rotate the MRU
		RecentFile rfCurrent = recentFileStack[0];
		// Move them up
		for (int stackPos = 0; stackPos < fileStackMax - 1; stackPos++) {
			recentFileStack[stackPos] = recentFileStack[stackPos + 1];
		}
		recentFileStack[fileStackMax - 1].Init();
		// Copy current file into first empty
		for (int emptyPos = 0; emptyPos < fileStackMax; emptyPos++) {
			if (!recentFileStack[emptyPos].IsSet()) {
				if (rfLast.IsSet()) {
					recentFileStack[emptyPos] = rfLast;
					rfLast.Init();
				} else {
					recentFileStack[emptyPos] = rfCurrent;
					break;
				}
			}
		}

		DeleteFileStackMenu();
		SetFileStackMenu();
	}
}

void SciTEBase::StackMenu(int pos) {
	if (CanMakeRoom(true)) {
		if (pos >= 0) {
			if ((pos == 0) && (!recentFileStack[pos].IsSet())) {	// Empty
				New();
				SetWindowName();
				ReadProperties();
				SetIndentSettings();
				SetEol();
			} else if (recentFileStack[pos].IsSet()) {
				RecentFile rf = recentFileStack[pos];
				// Already asked user so don't allow Open to ask again.
				Open(rf, ofNoSaveIfDirty);
				CurrentBuffer()->scrollPosition = rf.scrollPosition;
				CurrentBuffer()->selection = rf.selection;
				DisplayAround(rf);
			}
		}
	}
}

/*!-remove-[SubMenu]
void SciTEBase::RemoveToolsMenu() {
	for (int pos = 0; pos < toolMax; pos++) {
		DestroyMenuItem(menuTools, IDM_TOOLS + pos);
	}
}
*/

void SciTEBase::SetMenuItemLocalised(int menuNumber, int position, int itemID,
        const char *text, const char *mnemonic) {
	GUI::gui_string localised = localiser.Text(text);
	SetMenuItem(menuNumber, position, itemID, localised.c_str(), GUI::StringFromUTF8(mnemonic).c_str());
}

/*!
void SciTEBase::SetToolsMenu() {
	//command.name.0.*.py=Edit in PythonWin
	//command.0.*.py="c:\program files\python\pythonwin\pythonwin" /edit c:\coloreditor.py
	RemoveToolsMenu();
	int menuPos = TOOLS_START;
	for (int item = 0; item < toolMax; item++) {
		int itemID = IDM_TOOLS + item;
		SString prefix = "command.name.";
		prefix += SString(item);
		prefix += ".";
		SString commandName = props.GetNewExpand(prefix.c_str(), FileNameExt().AsUTF8().c_str());
		if (commandName.length()) {
			SString sMenuItem = commandName;
			prefix = "command.shortcut.";
			prefix += SString(item);
			prefix += ".";
			SString sMnemonic = props.GetNewExpand(prefix.c_str(), FileNameExt().AsUTF8().c_str());
			if (item < 10 && sMnemonic.length() == 0) {
				sMnemonic += "Ctrl+";
				sMnemonic += SString(item);
			}
			SetMenuItemLocalised(menuTools, menuPos, itemID, sMenuItem.c_str(),
				sMnemonic[0] ? sMnemonic.c_str() : NULL);
			menuPos++;
		}
	}

	DestroyMenuItem(menuTools, IDM_MACRO_SEP);
	DestroyMenuItem(menuTools, IDM_MACROLIST);
	DestroyMenuItem(menuTools, IDM_MACROPLAY);
	DestroyMenuItem(menuTools, IDM_MACRORECORD);
	DestroyMenuItem(menuTools, IDM_MACROSTOPRECORD);
	menuPos++;
	if (macrosEnabled) {
		SetMenuItem(menuTools, menuPos++, IDM_MACRO_SEP, GUI_TEXT(""));
		SetMenuItemLocalised(menuTools, menuPos++, IDM_MACROLIST,
		        "&List Macros...", "Shift+F9");
		SetMenuItemLocalised(menuTools, menuPos++, IDM_MACROPLAY,
		        "Run Current &Macro", "F9");
		SetMenuItemLocalised(menuTools, menuPos++, IDM_MACRORECORD,
		        "&Record Macro", "Ctrl+F9");
		SetMenuItemLocalised(menuTools, menuPos, IDM_MACROSTOPRECORD,
		        "S&top Recording Macro", "Ctrl+Shift+F9");
	}
}
*/
//!-start-[SubMenu]
void SciTEBase::SetToolsMenu() {
	int items;
	MenuEx arrMenu[toolMax];
	int menuPos = TOOLS_START+1;

	// erasing menu tools
	arrMenu[0] = GetToolsMenu();
	arrMenu[0].RemoveItems(IDM_TOOLS);
	arrMenu[0].RemoveItems(IDM_MACRO_SEP, IDM_MACROLIST);

	// menu creation
	for (items = 0; items < toolMax; items++) {
		int itemID = IDM_TOOLS + items;
		SString prefix = "command.name." + SString(items) + ".";
		SString sMenuItem = props.GetNewExpand(prefix.c_str(), FileNameExt().AsUTF8().c_str());
		if (sMenuItem.length()) {
			prefix = "command.shortcut." + SString(items) + ".";
			SString sMnemonic = props.GetNewExpand(prefix.c_str(), FileNameExt().AsUTF8().c_str());
			if (items < 10 && sMnemonic.length() == 0)
				sMnemonic += "Ctrl+" + SString(items);
			prefix = "command.separator." + SString(items) + ".";
			int issep = props.GetNewExpand(prefix.c_str(), FileNameExt().AsUTF8().c_str()).value();
			prefix = "command.parent." + SString(items) + ".";
			int toMenu = props.GetNewExpand(prefix.c_str(), FileNameExt().AsUTF8().c_str()).value();
			GUI::gui_string lsMenuItem = localiser.Text(sMenuItem.c_str());
			prefix = "command.checked." + SString(items) + ".";
			int ischecked = props.GetNewExpand(prefix.c_str(), FileNameExt().AsUTF8().c_str()).value();
			if(toMenu > 0 && toMenu < toolMax) {
				if (arrMenu[toMenu].GetID() == 0)
					arrMenu[toMenu].CreatePopUp(&arrMenu[0]);
				if (issep)
					arrMenu[toMenu].Add();
				arrMenu[toMenu].Add(lsMenuItem.c_str(), itemID, 1 + ischecked, sMnemonic.c_str());
			}
			else {
				if (issep)
					arrMenu[0].Add(0, IDM_TOOLSMAX, true, 0, menuPos++);
				arrMenu[0].Add(lsMenuItem.c_str(), itemID, 1 + ischecked, sMnemonic.c_str(), menuPos++);
			}
		}
	}

	// adding macro's menu items
	if (macrosEnabled) {
		SetMenuItem(menuTools, menuPos++, IDM_MACRO_SEP, GUI_TEXT(""));
		SetMenuItemLocalised(menuTools, menuPos++, IDM_MACROLIST,
			"&List Macros...", "Shift+F9");
		SetMenuItemLocalised(menuTools, menuPos++, IDM_MACROPLAY,
			"Run Current &Macro", "F9");
		SetMenuItemLocalised(menuTools, menuPos++, IDM_MACRORECORD,
			"&Record Macro", "Ctrl+F9");
		SetMenuItemLocalised(menuTools, menuPos++, IDM_MACROSTOPRECORD,
			"S&top Recording Macro", "Ctrl+Shift+F9");
	}

	// inserting submenus to the top
	menuPos = TOOLS_START+1;
	for (items = 1; items < toolMax; items++) {
		if (arrMenu[items].GetID() != 0) {
			SString prefix = "command.submenu.name." + SString(items) + ".";
			SString commandName = props.GetNewExpand(prefix.c_str(), filePath.AsUTF8().c_str());
			if (commandName.length()) {
				prefix = "command.submenu.parent." + SString(items) + ".";
				int toMenu = props.GetNewExpand(prefix.c_str(), FileNameExt().AsUTF8().c_str()).value();
				GUI::gui_string lcommandName = localiser.Text(commandName.c_str());
				if (toMenu > 0 && toMenu < toolMax) {
					arrMenu[toMenu].AddSubMenu(lcommandName.c_str(), arrMenu[items], 0);
				}
				else {
					if (menuPos == TOOLS_START+1)
						arrMenu[0].Add(0, IDM_TOOLSMAX, true, 0, menuPos++);
					arrMenu[0].AddSubMenu(lcommandName.c_str(), arrMenu[items], menuPos++);
				}
			}
		}
	}
}
//!-end-[SubMenu]

JobSubsystem SciTEBase::SubsystemType(char c) {
	if (c == '1')
		return jobGUI;
	else if (c == '2')
		return jobShell;
	else if (c == '3')
		return jobExtension;
	else if (c == '4')
		return jobHelp;
	else if (c == '5')
		return jobOtherHelp;
	return jobCLI;
}

JobSubsystem SciTEBase::SubsystemType(const char *cmd, int item) {
	SString subsysprefix = cmd;
	if (item >= 0) {
		subsysprefix += SString(item);
		subsysprefix += ".";
	}
	SString subsystem = props.GetNewExpand(subsysprefix.c_str(), FileNameExt().AsUTF8().c_str());
	return SubsystemType(subsystem[0]);
}

void SciTEBase::ToolsMenu(int item) {
	SelectionIntoProperties();

	SString itemSuffix = item;
	itemSuffix += '.';

	SString propName = "command.";
	propName += itemSuffix;

	SString command = props.GetWild(propName.c_str(), FileNameExt().AsUTF8().c_str());
	if (command.length()) {
		int saveBefore = 0;

		JobSubsystem jobType = jobCLI;
		bool isFilter = false;
		bool quiet = false;
		int repSel = 0;
		bool groupUndo = false;

		propName = "command.mode.";
		propName += itemSuffix;
		SString modeVal = props.GetNewExpand(propName.c_str(), FileNameExt().AsUTF8().c_str());
		modeVal.remove(" ");
		if (modeVal.length()) {
			char *modeTags = modeVal.detach();

			// copy/paste from style selectors.
			char *opt = modeTags;
			while (opt) {
				// Find attribute separator
				char *cpComma = strchr(opt, ',');
				if (cpComma) {
					// If found, we terminate the current attribute (opt) string
					*cpComma = '\0';
				}
				// Find attribute name/value separator
				char *colon = strchr(opt, ':');
				if (colon) {
					// If found, we terminate the current attribute name and point on the value
					*colon++ = '\0';
				}

				if (0 == strcmp(opt, "subsystem") && colon) {
					if (colon[0] == '0' || 0 == strcmp(colon, "console"))
						jobType = jobCLI;
					else if (colon[0] == '1' || 0 == strcmp(colon, "windows"))
						jobType = jobGUI;
					else if (colon[0] == '2' || 0 == strcmp(colon, "shellexec"))
						jobType = jobShell;
					else if (colon[0] == '3' || 0 == strcmp(colon, "lua") || 0 == strcmp(colon, "director"))
						jobType = jobExtension;
					else if (colon[0] == '4' || 0 == strcmp(colon, "htmlhelp"))
						jobType = jobHelp;
					else if (colon[0] == '5' || 0 == strcmp(colon, "winhelp"))
						jobType = jobOtherHelp;
				}

				if (0 == strcmp(opt, "quiet")) {
					if (!colon || colon[0] == '1' || 0 == strcmp(colon, "yes"))
						quiet = true;
					else if (colon[0] == '0' || 0 == strcmp(colon, "no"))
						quiet = false;
				}

				if (0 == strcmp(opt, "savebefore")) {
					if (!colon || colon[0] == '1' || 0 == strcmp(colon, "yes"))
						saveBefore = 1;
					else if (colon[0] == '0' || 0 == strcmp(colon, "no"))
						saveBefore = 2;
					else if (0 == strcmp(colon, "prompt"))
						saveBefore = 0;
				}

				if (0 == strcmp(opt, "filter")) {
					if (!colon || colon[0] == '1' || 0 == strcmp(colon, "yes"))
						isFilter = true;
					else if (colon[1] == '0' || 0 == strcmp(colon, "no"))
						isFilter = false;
				}

				if (0 == strcmp(opt, "replaceselection")) {
					if (!colon || colon[0] == '1' || 0 == strcmp(colon, "yes"))
						repSel = 1;
					else if (colon[0] == '0' || 0 == strcmp(colon, "no"))
						repSel = 0;
					else if (0 == strcmp(colon, "auto"))
						repSel = 2;
				}

				if (0 == strcmp(opt, "groupundo")) {
					if (!colon || colon[0] == '1' || 0 == strcmp(colon, "yes"))
						groupUndo = true;
					else if (colon[0] == '0' || 0 == strcmp(colon, "no"))
						groupUndo = false;
				}
//!-start-[clearbefore]
				if (0 == strcmp(opt, "clearbefore")) {
					if (!colon || colon[0] == '1' || 0 == strcmp(colon, "yes"))
						jobQueue.clearBeforeExecute = true;
					else if (colon[0] == '0' || 0 == strcmp(colon, "no"))
						jobQueue.clearBeforeExecute = false;
				}
//!-end-[clearbefore]

				opt = cpComma ? cpComma + 1 : 0;
			}
			delete []modeTags;
		}

		// The mode flags also have classic properties with similar effect.
		// If the classic property is specified, it overrides the mode.
		// To see if the property is absent (as opposed to merely evaluating
		// to nothing after variable expansion), use GetWild for the
		// existence check.  However, for the value check, use getNewExpand.

		propName = "command.save.before.";
		propName += itemSuffix;
		if (props.GetWild(propName.c_str(), FileNameExt().AsUTF8().c_str()).length())
			saveBefore = props.GetNewExpand(propName.c_str(), FileNameExt().AsUTF8().c_str()).value();

//!		if (saveBefore == 2 || (saveBefore == 1 && (!(CurrentBuffer()->isDirty) || Save())) || SaveIfUnsure() != IDCANCEL) {
		if (saveBefore == 2 || (saveBefore == 1 && (!(CurrentBuffer()->DocumentNotSaved()) || Save())) || SaveIfUnsure() != IDCANCEL) { //!-change-[OpenNonExistent]
			int flags = 0;

			propName = "command.is.filter.";
			propName += itemSuffix;
			if (props.GetWild(propName.c_str(), FileNameExt().AsUTF8().c_str()).length())
				isFilter = (props.GetNewExpand(propName.c_str(), FileNameExt().AsUTF8().c_str())[0] == '1');
			if (isFilter)
				CurrentBuffer()->fileModTime -= 1;

			propName = "command.subsystem.";
			propName += itemSuffix;
			if (props.GetWild(propName.c_str(), FileNameExt().AsUTF8().c_str()).length()) {
				SString subsystemVal = props.GetNewExpand(propName.c_str(), FileNameExt().AsUTF8().c_str());
				jobType = SubsystemType(subsystemVal[0]);
			}

			propName = "command.input.";
			propName += itemSuffix;
			SString input;
			if (props.GetWild(propName.c_str(), FileNameExt().AsUTF8().c_str()).length()) {
				input = props.GetNewExpand(propName.c_str(), FileNameExt().AsUTF8().c_str());
				flags |= jobHasInput;
			}

			propName = "command.quiet.";
			propName += itemSuffix;
			if (props.GetWild(propName.c_str(), FileNameExt().AsUTF8().c_str()).length())
				quiet = (props.GetNewExpand(propName.c_str(), FileNameExt().AsUTF8().c_str()).value() == 1);
			if (quiet)
				flags |= jobQuiet;

			propName = "command.replace.selection.";
			propName += itemSuffix;
			if (props.GetWild(propName.c_str(), FileNameExt().AsUTF8().c_str()).length())
				repSel = props.GetNewExpand(propName.c_str(), FileNameExt().AsUTF8().c_str()).value();

			if (repSel == 1)
				flags |= jobRepSelYes;
			else if (repSel == 2)
				flags |= jobRepSelAuto;

			if (groupUndo)
				flags |= jobGroupUndo;

			AddCommand(command, "", jobType, input, flags);
			if (jobQueue.commandCurrent > 0)
				Execute();
		}
	}
}

inline bool isdigitchar(int ch) {
	return (ch >= '0') && (ch <= '9');
}

int DecodeMessage(const char *cdoc, char *sourcePath, int format, int &column) {
	sourcePath[0] = '\0';
	column = -1; // default to not detected
	switch (format) {
	case SCE_ERR_PYTHON: {
			// Python
			const char *startPath = strchr(cdoc, '\"');
			if (startPath) {
				startPath++;
				const char *endPath = strchr(startPath, '\"');
				if (endPath) {
					ptrdiff_t length = endPath - startPath;
					if (length > 0) {
						strncpy(sourcePath, startPath, length);
						sourcePath[length] = 0;
					}
					endPath++;
					while (*endPath && !isdigitchar(*endPath)) {
						endPath++;
					}
					int sourceNumber = atoi(endPath) - 1;
					return sourceNumber;
				}
			}
		}
	case SCE_ERR_GCC: {
			// GCC - look for number after colon to be line number
			// This will be preceded by file name.
			// Lua debug traceback messages also piggyback this style, but begin with a tab.
			if (cdoc[0] == '\t')
				++cdoc;
			for (int i = 0; cdoc[i]; i++) {
				if (cdoc[i] == ':' && isdigitchar(cdoc[i + 1])) {
					int sourceNumber = atoi(cdoc + i + 1) - 1;
					if (i > 0) {
						strncpy(sourcePath, cdoc, i);
						sourcePath[i] = 0;
					}
					return sourceNumber;
				}
			}
			break;
		}
	case SCE_ERR_MS: {
			// Visual *
			const char *start = cdoc;
			while (isspacechar(*start)) {
				start++;
			}
			const char *endPath = strchr(start, '(');
			ptrdiff_t length = endPath - start;
			if ((length > 0) && (length < MAX_PATH)) {
				strncpy(sourcePath, start, length);
				sourcePath[length] = 0;
			}
			endPath++;
			return atoi(endPath) - 1;
		}
	case SCE_ERR_BORLAND: {
			// Borland
			const char *space = strchr(cdoc, ' ');
			if (space) {
				while (isspacechar(*space)) {
					space++;
				}
				while (*space && !isspacechar(*space)) {
					space++;
				}
				while (isspacechar(*space)) {
					space++;
				}

				const char *space2 = NULL;

				if (strlen(space) > 2) {
					space2 = strchr(space + 2, ':');
				}

				if (space2) {
					while (!isspacechar(*space2)) {
						space2--;
					}

					while (isspacechar(*(space2 - 1))) {
						space2--;
					}

					ptrdiff_t length = space2 - space;

					if (length > 0) {
						strncpy(sourcePath, space, length);
						sourcePath[length] = '\0';
						return atoi(space2) - 1;
					}
				}
			}
			break;
		}
	case SCE_ERR_PERL: {
			// perl
			const char *at = strstr(cdoc, " at ");
			const char *line = strstr(cdoc, " line ");
			ptrdiff_t length = line - (at + 4);
			if (at && line && length > 0) {
				strncpy(sourcePath, at + 4, length);
				sourcePath[length] = 0;
				line += 6;
				return atoi(line) - 1;
			}
			break;
		}
	case SCE_ERR_NET: {
			// .NET traceback
			const char *in = strstr(cdoc, " in ");
			const char *line = strstr(cdoc, ":line ");
			if (in && line && (line > in)) {
				in += 4;
				strncpy(sourcePath, in, line - in);
				sourcePath[line - in] = 0;
				line += 6;
				return atoi(line) - 1;
			}
		}
	case SCE_ERR_LUA: {
			// Lua 4 error looks like: last token read: `result' at line 40 in file `Test.lua'
			const char *idLine = "at line ";
			const char *idFile = "file ";
			size_t lenLine = strlen(idLine);
			size_t lenFile = strlen(idFile);
			const char *line = strstr(cdoc, idLine);
			const char *file = strstr(cdoc, idFile);
			if (line && file) {
				const char *fileStart = file + lenFile + 1;
				const char *quote = strstr(fileStart, "'");
				size_t length = quote - fileStart;
				if (quote && length > 0) {
					strncpy(sourcePath, fileStart, length);
					sourcePath[length] = '\0';
				}
				line += lenLine;
				return atoi(line) - 1;
			} else {
				// Lua 5.1 error looks like: lua.exe: test1.lua:3: syntax error
				// reuse the GCC error parsing code above!
				const char* colon = strstr(cdoc, ": ");
				if (cdoc)
					return DecodeMessage(colon + 2, sourcePath, SCE_ERR_GCC, column);
			}
			break;
		}

	case SCE_ERR_CTAG: {
			for (int i = 0; cdoc[i]; i++) {
				if ((isdigitchar(cdoc[i + 1]) || (cdoc[i + 1] == '/' && cdoc[i + 2] == '^')) && cdoc[i] == '\t') {
					int j = i - 1;
					while (j > 0 && ! strchr("\t\n\r \"$%'*,;<>?[]^`{|}", cdoc[j])) {
						j--;
					}
					if (strchr("\t\n\r \"$%'*,;<>?[]^`{|}", cdoc[j])) {
						j++;
					}
					strncpy(sourcePath, &cdoc[j], i - j);
					sourcePath[i - j] = 0;
					// Because usually the address is a searchPattern, lineNumber has to be evaluated later
					return 0;
				}
			}
		}
	case SCE_ERR_PHP: {
			// PHP error look like: Fatal error: Call to undefined function:  foo() in example.php on line 11
			const char *idLine = " on line ";
			const char *idFile = " in ";
			size_t lenLine = strlen(idLine);
			size_t lenFile = strlen(idFile);
			const char *line = strstr(cdoc, idLine);
			const char *file = strstr(cdoc, idFile);
			if (line && file && (line > file)) {
				file += lenFile;
				size_t length = line - file;
				strncpy(sourcePath, file, length);
				sourcePath[length] = '\0';
				line += lenLine;
				return atoi(line) - 1;
			}
			break;
		}

	case SCE_ERR_ELF: {
			// Essential Lahey Fortran error look like: Line 11, file c:\fortran90\codigo\demo.f90
			const char *line = strchr(cdoc, ' ');
			if (line) {
				while (isspacechar(*line)) {
					line++;
				}
				const char *file = strchr(line, ' ');
				if (file) {
					while (isspacechar(*file)) {
						file++;
					}
					while (*file && !isspacechar(*file)) {
						file++;
					}
					size_t length = strlen(file);
					strncpy(sourcePath, file, length);
					sourcePath[length] = '\0';
					return atoi(line) - 1;
				}
			}
			break;
		}

	case SCE_ERR_IFC: {
			/* Intel Fortran Compiler error/warnings look like:
			 * Error 71 at (17:teste.f90) : The program unit has no name
			 * Warning 4 at (9:modteste.f90) : Tab characters are an extension to standard Fortran 95
			 *
			 * Depending on the option, the error/warning messages can also appear on the form:
			 * modteste.f90(9): Warning 4 : Tab characters are an extension to standard Fortran 95
			 *
			 * These are trapped by the MS handler, and are identified OK, so no problem...
			 */
			const char *line = strchr(cdoc, '(');
			const char *file = strchr(line, ':');
			if (line && file) {
				file++;
				const char *endfile = strchr(file, ')');
				size_t length = endfile - file;
				strncpy(sourcePath, file, length);
				sourcePath[length] = '\0';
				line++;
				return atoi(line) - 1;
			}
			break;
		}

	case SCE_ERR_ABSF: {
			// Absoft Pro Fortran 90/95 v8.x, v9.x  errors look like: cf90-113 f90fe: ERROR SHF3D, File = shf.f90, Line = 1101, Column = 19
			const char *idFile = " File = ";
			const char *idLine = ", Line = ";
			size_t lenFile = strlen(idFile);
			size_t lenLine = strlen(idLine);
			const char *file = strstr(cdoc, idFile);
			const char *line = strstr(cdoc, idLine);
			//const char *idColumn = ", Column = ";
			//const char *column = strstr(cdoc, idColumn);
			if (line && file && (line > file)) {
				file += lenFile;
				size_t length = line - file;
				strncpy(sourcePath, file, length);
				sourcePath[length] = '\0';
				line += lenLine;
				return atoi(line) - 1;
			}
			break;
		}

	case SCE_ERR_IFORT: {
			/* Intel Fortran Compiler v8.x error/warnings look like:
			 * fortcom: Error: shf.f90, line 5602: This name does not have ...
				 */
			const char *idFile = ": Error: ";
			const char *idLine = ", line ";
			size_t lenFile = strlen(idFile);
			size_t lenLine = strlen(idLine);
			const char *file = strstr(cdoc, idFile);
			const char *line = strstr(cdoc, idLine);
			const char *lineend = strrchr(cdoc, ':');
			if (line && file && (line > file)) {
				file += lenFile;
				size_t length = line - file;
				strncpy(sourcePath, file, length);
				sourcePath[length] = '\0';
				line += lenLine;
				if ((lineend > line)) {
					return atoi(line) - 1;
				}
			}
			break;
		}

	case SCE_ERR_TIDY: {
			/* HTML Tidy error/warnings look like:
			 * line 8 column 1 - Error: unexpected </head> in <meta>
			 * line 41 column 1 - Warning: <table> lacks "summary" attribute
			 */
			const char *line = strchr(cdoc, ' ');
			if (line) {
				const char *col = strchr(line + 1, ' ');
				if (col) {
					//*col = '\0';
					int lnr = atoi(line) - 1;
					col = strchr(col + 1, ' ');
					if (col) {
						const char *endcol = strchr(col + 1, ' ');
						if (endcol) {
							//*endcol = '\0';
							column = atoi(col) - 1;
							return lnr;
						}
					}
				}
			}
			break;
		}

	case SCE_ERR_JAVA_STACK: {
			/* Java runtime stack trace
				\tat <methodname>(<filename>:<line>)
				 */
			const char *startPath = strrchr(cdoc, '(') + 1;
			const char *endPath = strchr(startPath, ':');
			ptrdiff_t length = endPath - startPath;
			if (length > 0) {
				strncpy(sourcePath, startPath, length);
				sourcePath[length] = 0;
				int sourceNumber = atoi(endPath + 1) - 1;
				return sourceNumber;
			}
			break;
		}

	case SCE_ERR_DIFF_MESSAGE: {
			// Diff file header, either +++ <filename>\t or --- <filename>\t
			// Often followed by a position line @@ <linenumber>
			const char *startPath = cdoc + 4;
			const char *endPath = strchr(startPath, '\t');
			if (endPath) {
				ptrdiff_t length = endPath - startPath;
				strncpy(sourcePath, startPath, length);
				sourcePath[length] = 0;
				return 0;
			}
			break;
		}
	}	// switch
	return -1;
}

// Remove up to and including ch
static void Chomp(SString &s, int ch) {
	if (s.contains(static_cast<char>(ch)))
			s.remove(0, s.search(":") + 1);
}

void SciTEBase::ShowMessages(int line) {
	wEditor.Call(SCI_ANNOTATIONSETSTYLEOFFSET, 256);
	wEditor.Call(SCI_ANNOTATIONSETVISIBLE, ANNOTATION_BOXED);
	wEditor.Call(SCI_ANNOTATIONCLEARALL);
	TextReader acc(wOutput);
	while ((line > 0) && (acc.StyleAt(acc.LineStart(line-1)) != SCE_ERR_CMD))
		line--;
	int maxLine = wOutput.Call(SCI_GETLINECOUNT);
	while ((line < maxLine) && (acc.StyleAt(acc.LineStart(line)) != SCE_ERR_CMD)) {
		int startPosLine = wOutput.Call(SCI_POSITIONFROMLINE, line, 0);
		int lineEnd = wOutput.Call(SCI_GETLINEENDPOSITION, line, 0);
		SString message = GetRange(wOutput, startPosLine, lineEnd);
		char source[MAX_PATH];	
		int column;
		char style = acc.StyleAt(startPosLine);
		int sourceLine = DecodeMessage(message.c_str(), source, style, column);
		Chomp(message, ':');
		if (style == SCE_ERR_GCC) {
			Chomp(message, ':');
		}
		GUI::gui_string sourceString = GUI::StringFromUTF8(source);
		FilePath sourcePath = FilePath(sourceString).NormalizePath();
		if (filePath.Name().SameNameAs(sourcePath.Name())) {
			if (style == SCE_ERR_GCC) {
				const char *sColon = strchr(message.c_str(), ':');
				if (sColon) {
					SString editLine = GetLine(wEditor, sourceLine);
					if (editLine == (sColon+1)) {
						line++;
						continue;
					}
				}
			}
			int lenCurrent = wEditor.CallString(SCI_ANNOTATIONGETTEXT, sourceLine, NULL);
			std::string msgCurrent(lenCurrent, '\0');
			std::string stylesCurrent(lenCurrent, '\0');
			if (lenCurrent) {
				wEditor.CallString(SCI_ANNOTATIONGETTEXT, sourceLine, &msgCurrent[0]);
				wEditor.CallString(SCI_ANNOTATIONGETSTYLES, sourceLine, &stylesCurrent[0]);
				msgCurrent += "\n";
				stylesCurrent += '\0';
			}
			msgCurrent += message.c_str();
			int msgStyle = 0;
			if (message.search("warning") >= 0)
				msgStyle = 1;
			if (message.search("error") >= 0)
				msgStyle = 2;
			if (message.search("fatal") >= 0)
				msgStyle = 3;
			stylesCurrent += std::string(message.length(), static_cast<char>(msgStyle));
			wEditor.CallString(SCI_ANNOTATIONSETTEXT, sourceLine, msgCurrent.c_str());
			wEditor.CallString(SCI_ANNOTATIONSETSTYLES, sourceLine, stylesCurrent.c_str());
		}
		line++;
	}
}

//!void SciTEBase::GoMessage(int dir) {
bool SciTEBase::GoMessage(int dir) { //!-change-[GoMessageImprovement]
	Sci_CharacterRange crange;
	crange.cpMin = wOutput.Call(SCI_GETSELECTIONSTART);
	crange.cpMax = wOutput.Call(SCI_GETSELECTIONEND);
	long selStart = crange.cpMin;
	int curLine = wOutput.Call(SCI_LINEFROMPOSITION, selStart);
	int maxLine = wOutput.Call(SCI_GETLINECOUNT);
	int lookLine = curLine + dir;
	if (lookLine < 0)
		lookLine = maxLine - 1;
	else if (lookLine >= maxLine)
		lookLine = 0;
	TextReader acc(wOutput);
	while ((dir == 0) || (lookLine != curLine)) {
		int startPosLine = wOutput.Call(SCI_POSITIONFROMLINE, lookLine, 0);
		int lineLength = wOutput.Call(SCI_LINELENGTH, lookLine, 0);
		char style = acc.StyleAt(startPosLine);
		if (style != SCE_ERR_DEFAULT &&
		        style != SCE_ERR_CMD &&
		        style != SCE_ERR_DIFF_ADDITION &&
		        style != SCE_ERR_DIFF_CHANGED &&
		        style != SCE_ERR_DIFF_DELETION) {
			wOutput.Call(SCI_MARKERDELETEALL, static_cast<uptr_t>(-1));
			wOutput.Call(SCI_MARKERDEFINE, 0, SC_MARK_SMALLRECT);
			wOutput.Call(SCI_MARKERSETFORE, 0, ColourOfProperty(props,
			        "error.marker.fore", ColourRGB(0x7f, 0, 0)));
			wOutput.Call(SCI_MARKERSETBACK, 0, ColourOfProperty(props,
//!			        "error.marker.back", ColourRGB(0xff, 0xff, 0)));
			        "error.line.back", ColourOfProperty(props, "error.marker.back", ColourRGB(0xff, 0xff, 0)))); //!-change-[ErrorLineBack]
			wOutput.Call(SCI_MARKERADD, lookLine, 0);
			wOutput.Call(SCI_SETSEL, startPosLine, startPosLine);
			SString message = GetRange(wOutput, startPosLine, startPosLine + lineLength);
			char source[MAX_PATH];
			int column;
			long sourceLine = DecodeMessage(message.c_str(), source, style, column);
			if (sourceLine >= 0) {
//!-start-[FindResultListStyle]
				if (props.GetInt("lexer.errorlist.findliststyle", 1)&& '.' == source[0] && pathSepChar == source[1]) {
					// it can be internal search result line, so try to find the base path in topic
					int topLine = lookLine - 1;
					SString topic;
					while (topLine >= 0) {
						int startPos = wOutput.Call(SCI_POSITIONFROMLINE, topLine, 0);
						int lineLen = wOutput.Call(SCI_LINELENGTH, topLine, 0);
						topic = GetRange(wOutput, startPos, startPos + lineLen);
						if ('>' == topic[0]) {
							break;
						} else {
							topic.clear();
							topLine--;
						}
					}
					if (topic.length() > 0 && 0 == strncmp(">Internal search",topic.c_str(),16)) {
						// get base path from topic text
						int toPos = topic.length() - 1;
						while (toPos >= 0 && pathSepChar != topic[toPos]) toPos--;
						int fromPos = toPos - 1;
						while (fromPos >= 0 && '"' != topic[fromPos]) fromPos--;
						if (fromPos > 0) {
							SString path = topic.substr(fromPos + 1, toPos - fromPos - 1);
							path.append(source + 1);
							strncpy(source, path.c_str(), MAX_PATH);
						}
					}
				}
//!-end-[FindResultListStyle]
				GUI::gui_string sourceString = GUI::StringFromUTF8(source);
				FilePath sourcePath = FilePath(sourceString).NormalizePath();
//!				if (!filePath.Name().SameNameAs(sourcePath)) {
				if (sourcePath.IsSet() && !filePath.Name().SameNameAs(sourcePath)) { //!-change-[GoMessageFix]
					FilePath messagePath;
					bool bExists = false;
					if (Exists(dirNameAtExecute.AsInternal(), sourceString.c_str(), &messagePath)) {
						bExists = true;
					} else if (Exists(dirNameForExecute.AsInternal(), sourceString.c_str(), &messagePath)) {
						bExists = true;
					} else if (Exists(filePath.Directory().AsInternal(), sourceString.c_str(), &messagePath)) {
						bExists = true;
					} else if (Exists(NULL, sourceString.c_str(), &messagePath)) {
						bExists = true;
					} else {
						// Look through buffers for name match
						for (int i = buffers.lengthVisible - 1; i >= 0; i--) {
							if (sourcePath.Name().SameNameAs(buffers.buffers[i].Name())) {
								messagePath = buffers.buffers[i];
								bExists = true;
							}
						}
					}
					if (bExists) {
						if (!Open(messagePath, ofSynchronous)) {
//!							return;
							return false; //!-change-[GoMessageImprovement]
						}
						CheckReload();
					}
				}

				// If ctag then get line number after search tag or use ctag line number
				if (style == SCE_ERR_CTAG) {
					char cTag[200];
					//without following focus GetCTag wouldn't work correct
					WindowSetFocus(wOutput);
					GetCTag(cTag, 200);
					if (cTag[0] != '\0') {
						if (atol(cTag) > 0) {
							//if tag is linenumber, get line
							sourceLine = atol(cTag) - 1;
						} else {
							findWhat = cTag;
							FindNext(false);
							//get linenumber for marker from found position
							sourceLine = wEditor.Call(SCI_LINEFROMPOSITION, wEditor.Call(SCI_GETCURRENTPOS));
						}
					}
				}

				if (props.GetInt("error.inline")) {
					ShowMessages(lookLine);
				}

				wEditor.Call(SCI_MARKERDELETEALL, 0);
				wEditor.Call(SCI_MARKERDEFINE, 0, SC_MARK_CIRCLE);
				wEditor.Call(SCI_MARKERSETFORE, 0, ColourOfProperty(props,
				        "error.marker.fore", ColourRGB(0x7f, 0, 0)));
				wEditor.Call(SCI_MARKERSETBACK, 0, ColourOfProperty(props,
				        "error.marker.back", ColourRGB(0xff, 0xff, 0)));
				wEditor.Call(SCI_MARKERADD, sourceLine, 0);
				int startSourceLine = wEditor.Call(SCI_POSITIONFROMLINE, sourceLine, 0);
				int endSourceline = wEditor.Call(SCI_POSITIONFROMLINE, sourceLine + 1, 0);
				if (column >= 0) {
					// Get the position in line according to current tab setting
					startSourceLine = wEditor.Call(SCI_FINDCOLUMN, sourceLine, column);
				}
				EnsureRangeVisible(startSourceLine, startSourceLine);
				if (props.GetInt("error.select.line") == 1) {
					//select whole source source line from column with error
					SetSelection(endSourceline, startSourceLine);
				} else {
					//simply move cursor to line, don't do any selection
					SetSelection(startSourceLine, startSourceLine);
				}
				message.substitute('\t', ' ');
				message.remove("\n");
				props.Set("CurrentMessage", message.c_str());
				UpdateStatusBar(false);
				WindowSetFocus(wEditor);
				return true; //!-add-[GoMessageImprovement]
//!			return;
			return false; //!-change-[GoMessageImprovement]
			}
		}
		lookLine += dir;
		if (lookLine < 0)
			lookLine = maxLine - 1;
		else if (lookLine >= maxLine)
			lookLine = 0;
		if (dir == 0)
//!			return;
			return false; //!-change-[GoMessageImprovement]
	}
	return false; //!-add-[GoMessageImprovement]
}
