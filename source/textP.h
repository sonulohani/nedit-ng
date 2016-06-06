/*******************************************************************************
*                                                                              *
* textP.h -- Nirvana Editor Text Editing Widget private include file           *
*                                                                              *
* Copyright 2003 The NEdit Developers                                          *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute versions of this program linked to  *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for    *
* more details.                                                                *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *
*                                                                              *
* Nirvana Text Editor                                                          *
* July 31, 2001                                                                *
*                                                                              *
*******************************************************************************/

#ifndef TEXTP_H_
#define TEXTP_H_

#include "TextBuffer.h"
#include "Point.h"

#include <X11/Intrinsic.h>
#include <X11/Xlib.h>
#include <X11/X.h>
#include <Xm/XmP.h>
#include <Xm/PrimitiveP.h>
#include <X11/CoreP.h>

class TextDisplay;

enum DragStates {
	NOT_CLICKED,
	PRIMARY_CLICKED,
	SECONDARY_CLICKED,
	CLICKED_IN_SELECTION,
	PRIMARY_DRAG,
	PRIMARY_RECT_DRAG,
	SECONDARY_DRAG,
	SECONDARY_RECT_DRAG,
	PRIMARY_BLOCK_DRAG,
	DRAG_CANCELED,
	MOUSE_PAN
};

enum MultiClickStates {
	NO_CLICKS,
	ONE_CLICK,
	TWO_CLICKS,
	THREE_CLICKS
};

struct TextClassPart {
	int ignore;
};

struct TextClassRec {
	CoreClassPart        core_class;
	XmPrimitiveClassPart primitive_class;
	TextClassPart        text_class;
};

extern TextClassRec nTextClassRec;

class TextPart {
public:
	// resources
	Pixel          P_selectFGPixel;
	Pixel          P_selectBGPixel;
	Pixel          P_highlightFGPixel;
	Pixel          P_highlightBGPixel;
	Pixel          P_cursorFGPixel;
	Pixel          P_lineNumFGPixel;
	Pixel          P_calltipFGPixel;
	Pixel          P_calltipBGPixel;
	XFontStruct *  P_fontStruct;
	Boolean        P_pendingDelete;
	Boolean        P_autoShowInsertPos;
	Boolean        P_autoWrap;
	Boolean        P_autoWrapPastedText;
	Boolean        P_continuousWrap;
	Boolean        P_autoIndent;
	Boolean        P_smartIndent;
	Boolean        P_overstrike;
	Boolean        P_heavyCursor;
	Boolean        P_readOnly;
	Boolean        P_hidePointer;
	int            P_rows;
	int            P_columns;
	int            P_marginWidth;
	int            P_marginHeight;
	int            P_cursorBlinkRate;
	int            P_wrapMargin;
	int            P_emulateTabs;
	int            P_lineNumCols;
	char *         P_delimiters;
	Cardinal       P_cursorVPadding;
	Widget         P_hScrollBar;
	Widget         P_vScrollBar;

	/* was private, but looks like a resource */
	XmString P_backlightCharTypes;   // background class string to parse


	/* private state */
	TextDisplay *textD;             // Pointer to display information
	int anchor;                     // Anchor for drag operations
	int rectAnchor;                 // Anchor for rectangular drag operations
	int dragState;                  // Why is the mouse being dragged and what is being acquired
	int multiClickState;            // How long is this multi-click sequence so far
	Point btnDownCoord;             // Mark the position of last btn down action for deciding when to begin paying attention to motion actions, and where to paste columns
	Time lastBtnDown;               // Timestamp of last button down event for multi-click recognition
	Point mouseCoord;               // Last known mouse position in drag operation (for autoscroll)
	bool selectionOwner;            // True if widget owns the selection
	int motifDestOwner;             // " " owns the motif destination
	int emTabsBeforeCursor;         // If non-zero, number of consecutive emulated tabs just entered.  Saved so chars can be deleted as a unit
	XtIntervalId autoScrollProcID;  // id of Xt timer proc for autoscroll
	XtIntervalId cursorBlinkProcID; // id of timer proc for cursor blink
	TextBuffer *dragOrigBuf;        // backup buffer copy used during block dragging of selections
	int dragXOffset, dragYOffset;   // offsets between cursor location and actual insertion point in drag
	int dragType;                   // style of block drag operation
	int dragInsertPos;              // location where text being block dragged was last inserted
	int dragRectStart;              // rect. offset ""
	int dragInserted;               // # of characters inserted at drag destination in last drag position
	int dragDeleted;                // # of characters deleted ""
	int dragSourceDeletePos;        // location from which move source text was removed at start of drag
	int dragSourceInserted;         // # of chars. inserted when move source text was deleted
	int dragSourceDeleted;          // # of chars. deleted ""
	int dragNLines;                 // # of newlines in text being drag'd
};

struct TextRec {
	CorePart        core;
	XmPrimitivePart primitive;
	TextPart        text;
};

#endif
