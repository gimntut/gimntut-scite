// Scintilla source code edit control
/** @file CallTip.h
 ** Interface to the call tip control.
 **/
// Copyright 1998-2001 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#ifndef CALLTIP_H
#define CALLTIP_H

#ifdef SCI_NAMESPACE
namespace Scintilla {
#endif

/**
 */
class CallTip {
/*!
	int startHighlight;    // character offset to start and...
	int endHighlight;      // ...end of highlighted text
*/
//!-start-[BetterCalltips]
	bool highlightChanged;              // flag to indicate that highlight ranges were changed
	SplitVector<int> startHighlight;    // character offset to start and...
	SplitVector<int> endHighlight;      // ...end of highlighted text
//!-end-[BetterCalltips]
	char *val;
	Font font;
	PRectangle rectUp;      // rectangle of last up angle in the tip
	PRectangle rectDown;    // rectangle of last down arrow in the tip
	int lineHeight;         // vertical line spacing
	int offsetMain;         // The alignment point of the call tip
	int tabSize;            // Tab size in pixels, <=0 no TAB expand
	bool useStyleCallTip;   // if true, STYLE_CALLTIP should be used
	int wrapBound;          // calltip wrap bound in chars, 0 - no wrap //!-add-[BetterCalltips]
	bool above;		// if true, display calltip above text

	// Private so CallTip objects can not be copied
	CallTip(const CallTip &);
	CallTip &operator=(const CallTip &);
	void DrawChunk(Surface *surface, int &x, const char *s,
		int posStart, int posEnd, int ytext, PRectangle rcClient,
		bool highlight, bool draw);
//!	int PaintContents(Surface *surfaceWindow, bool draw);
	PRectangle PaintContents(Surface *surfaceWindow, bool draw); //!-change-[BetterCalltips]
	bool IsTabCharacter(char c) const;
	int NextTabPos(int x);
	void WrapLine(const char *text, int offset, int length, SplitVector<int> &wrapPosList); //!-add-[BetterCalltips]

public:
	Window wCallTip;
	Window wDraw;
	bool inCallTipMode;
	int posStartCallTip;
	ColourDesired colourBG;
	ColourDesired colourUnSel;
	ColourDesired colourSel;
	ColourDesired colourShade;
	ColourDesired colourLight;
	int codePage;
	int clickPlace;

	CallTip();
	~CallTip();

	void PaintCT(Surface *surfaceWindow);

	void MouseClick(Point pt);

	/// Setup the calltip and return a rectangle of the area required.
	PRectangle CallTipStart(int pos, Point pt, int textHeight, const char *defn,
		const char *faceName, int size, int codePage_,
		int characterSet, int technology, Window &wParent);

	void CallTipCancel();

	/// Set a range of characters to be displayed in a highlight style.
	/// Commonly used to highlight the current parameter.
	void SetHighlight(int start, int end);
//!-start-[BetterCalltips]
	/// Add a range of characters to be displayed in a highlight style.
	void AddHighlight(int start, int end);
	/// Delete all highlighted ranges
	void ClearHighlight();
	/// Update calltip window to reflect changes made by AddHighlight() and ClearHighlight()
	void UpdateHighlight();
//!-end-[BetterCalltips]

	/// Set the tab size in pixels for the call tip. 0 or -ve means no tab expand.
	void SetTabSize(int tabSz);

	/// Set calltip position.
	void SetPosition(bool aboveText);

	/// Used to determine which STYLE_xxxx to use for call tip information
	bool UseStyleCallTip() const { return useStyleCallTip;}

	// Modify foreground and background colours
//!-start-[BetterCalltips]
	// Set calltip line wrap bound in characters, 0 means no wrap
	void SetWrapBound(int wrapBnd);
//!-end-[BetterCalltips]
	void SetForeBack(const ColourDesired &fore, const ColourDesired &back);
};

#ifdef SCI_NAMESPACE
}
#endif

#endif
