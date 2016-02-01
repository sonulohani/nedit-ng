
#include "Document.h"
#include "file.h"
#include "highlight.h"
#include "interpret.h"
#include "macro.h"
#include "menu.h"
#include "nedit.bm"
#include "preferences.h"
#include "RangesetTable.h"
#include "search.h"
#include "selection.h"
#include "server.h"
#include "shell.h"
#include "smartIndent.h"
#include "textDisp.h"
#include "text.h"
#include "textP.h"
#include "UndoInfo.h"
#include "userCmds.h"
#include "window.h" // There are a few global functions found here... for now
#include "windowTitle.h"
#include "textSel.h"

#include "../Microline/XmL/Folder.h"
#include "../util/clearcase.h"
#include "../util/DialogF.h"
#include "../util/fileUtils.h"
#include "../util/misc.h"
#include "../Xlt/BubbleButton.h"
#include "../Xlt/BubbleButtonP.h"

#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/Xatom.h>
#include <Xm/CascadeB.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/Label.h>
#include <Xm/List.h>
#include <Xm/MainW.h>
#include <Xm/PanedW.h>
#include <Xm/PanedWP.h>
#include <Xm/PrimitiveP.h>
#include <Xm/Protocols.h>
#include <Xm/PushB.h>
#include <Xm/RowColumnP.h>
#include <Xm/ScrollBar.h>
#include <Xm/ScrolledW.h>
#include <Xm/SelectioB.h>
#include <Xm/Separator.h>
#include <Xm/Text.h>
#include <Xm/ToggleB.h>
#include <Xm/Xm.h>

namespace {

/* bitmap data for the close-tab button */
const int close_width  = 11;
const int close_height = 11;
static unsigned char close_bits[] = {0x00, 0x00, 0x00, 0x00, 0x8c, 0x01, 0xdc, 0x01, 0xf8, 0x00, 0x70, 0x00, 0xf8, 0x00, 0xdc, 0x01, 0x8c, 0x01, 0x00, 0x00, 0x00, 0x00};

/* bitmap data for the isearch-find button */
const int isrcFind_width  = 11;
const int isrcFind_height = 11;
static unsigned char isrcFind_bits[] = {0xe0, 0x01, 0x10, 0x02, 0xc8, 0x04, 0x08, 0x04, 0x08, 0x04, 0x00, 0x04, 0x18, 0x02, 0xdc, 0x01, 0x0e, 0x00, 0x07, 0x00, 0x03, 0x00};

/* bitmap data for the isearch-clear button */
const int isrcClear_width  = 11;
const int isrcClear_height = 11;
static unsigned char isrcClear_bits[] = {0x00, 0x00, 0x00, 0x00, 0x04, 0x01, 0x84, 0x01, 0xc4, 0x00, 0x64, 0x00, 0xc4, 0x00, 0x84, 0x01, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00};

/* Thickness of 3D border around statistics and/or incremental search areas
   below the main menu bar */
const int STAT_SHADOW_THICKNESS = 1;

/* Initial minimum height of a pane.  Just a fallback in case setPaneMinHeight
   (which may break in a future release) is not available */
const int PANE_MIN_HEIGHT = 39;

/* From Xt, Shell.c, "BIGSIZE" */
const Dimension XT_IGNORE_PPOSITION = 32767;

Document *inFocusDocument   = nullptr; /* where we are now */
Document *lastFocusDocument = nullptr; /* where we came from */

int DoneWithMoveDocumentDialog;


/*
** perform generic management on the children (toolbars) of toolBarsForm,
** a.k.a. statsForm, by setting the form attachment of the managed child
** widgets per their position/order.
**
** You can optionally create separator after a toolbar widget with it's
** widget name set to "TOOLBAR_SEP", which will appear below the toolbar
** widget. These seperators will then be managed automatically by this
** routine along with the toolbars they 'attached' to.
**
** It also takes care of the attachment offset settings of the child
** widgets to keep the border lines of the parent form displayed, so
** you don't have set them before hand.
**
** Note: XtManage/XtUnmange the target child (toolbar) before calling this
**       function.
**
** Returns the last toolbar widget managed.
**
*/
Widget manageToolBars(Widget toolBarsForm) {
	Widget topWidget = nullptr;
	WidgetList children;
	int n, nItems = 0;

	XtVaGetValues(toolBarsForm, XmNchildren, &children, XmNnumChildren, &nItems, nullptr);

	for (n = 0; n < nItems; n++) {
		Widget tbar = children[n];

		if (XtIsManaged(tbar)) {
			if (topWidget) {
				XtVaSetValues(tbar, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, topWidget, XmNbottomAttachment, XmATTACH_NONE, XmNleftOffset, STAT_SHADOW_THICKNESS, XmNrightOffset, STAT_SHADOW_THICKNESS, nullptr);
			} else {
				/* the very first toolbar on top */
				XtVaSetValues(tbar, XmNtopAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_NONE, XmNleftOffset, STAT_SHADOW_THICKNESS, XmNtopOffset, STAT_SHADOW_THICKNESS, XmNrightOffset, STAT_SHADOW_THICKNESS, nullptr);
			}

			topWidget = tbar;

			/* if the next widget is a separator, turn it on */
			if (n + 1 < nItems && !strcmp(XtName(children[n + 1]), "TOOLBAR_SEP")) {
				XtManageChild(children[n + 1]);
			}
		} else {
			/* Remove top attachment to widget to avoid circular dependency.
			   Attach bottom to form so that when the widget is redisplayed
			   later, it will trigger the parent form to resize properly as
			   if the widget is being inserted */
			XtVaSetValues(tbar, XmNtopAttachment, XmATTACH_NONE, XmNbottomAttachment, XmATTACH_FORM, nullptr);

			/* if the next widget is a separator, turn it off */
			if (n + 1 < nItems && !strcmp(XtName(children[n + 1]), "TOOLBAR_SEP")) {
				XtUnmanageChild(children[n + 1]);
			}
		}
	}

	if (topWidget) {
		if (strcmp(XtName(topWidget), "TOOLBAR_SEP")) {
			XtVaSetValues(topWidget, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, STAT_SHADOW_THICKNESS, nullptr);
		} else {
			/* is a separator */
			Widget wgt;
			XtVaGetValues(topWidget, XmNtopWidget, &wgt, nullptr);

			/* don't need sep below bottom-most toolbar */
			XtUnmanageChild(topWidget);
			XtVaSetValues(wgt, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, STAT_SHADOW_THICKNESS, nullptr);
		}
	}

	return topWidget;
}

/*
** Paned windows are impossible to adjust after they are created, which makes
** them nearly useless for NEdit (or any application which needs to dynamically
** adjust the panes) unless you tweek some private data to overwrite the
** desired and minimum pane heights which were set at creation time.  These
** will probably break in a future release of Motif because of dependence on
** private data.
*/
void setPaneDesiredHeight(Widget w, int height) {
	reinterpret_cast<XmPanedWindowConstraintPtr>(w->core.constraints)->panedw.dheight = height;
}

/*
**
*/
void setPaneMinHeight(Widget w, int min) {
	reinterpret_cast<XmPanedWindowConstraintPtr>(w->core.constraints)->panedw.min = min;
}

/*
**
*/
Widget containingPane(Widget w) {
	/* The containing pane used to simply be the first parent, but with
	   the introduction of an XmFrame, it's the grandparent. */
	return XtParent(XtParent(w));
}

/*
**
*/
void moveDocumentCB(Widget dialog, XtPointer clientData, XtPointer callData) {

	(void)dialog;
	(void)clientData;

	auto cbs = static_cast<XmSelectionBoxCallbackStruct *>(callData);
	DoneWithMoveDocumentDialog = cbs->reason;
}

/*
** Xt timer procedure for updating size hints.  The new sizes of objects in
** the window are not ready immediately after adding or removing panes.  This
** is a timer routine to be invoked with a timeout of 0 to give the event
** loop a chance to finish processing the size changes before reading them
** out for setting the window manager size hints.
*/
void wmSizeUpdateProc(XtPointer clientData, XtIntervalId *id) {

	(void)id;
	static_cast<Document *>(clientData)->UpdateWMSizeHints();
}

/*
**
*/
void cancelTimeOut(XtIntervalId *timer) {
	if (*timer != 0) {
		XtRemoveTimeOut(*timer);
		*timer = 0;
	}
}

/*
**
*/
void modifiedCB(int pos, int nInserted, int nDeleted, int nRestyled, view::string_view deletedText, void *cbArg) {

	(void)nRestyled;

	Document *window = (Document *)cbArg;
	int selected = window->buffer->primary_.selected;

	/* update the table of bookmarks */
	if (!window->ignoreModify) {
		UpdateMarkTable(window, pos, nInserted, nDeleted);
	}

	/* Check and dim/undim selection related menu items */
	if ((window->wasSelected && !selected) || (!window->wasSelected && selected)) {
		window->wasSelected = selected;

		/* do not refresh shell-level items (window, menu-bar etc)
		   when motifying non-top document */
		if (window->IsTopDocument()) {
			XtSetSensitive(window->printSelItem, selected);
			XtSetSensitive(window->cutItem, selected);
			XtSetSensitive(window->copyItem, selected);
			XtSetSensitive(window->delItem, selected);
			/* Note we don't change the selection for items like
			   "Open Selected" and "Find Selected".  That's because
			   it works on selections in external applications.
			   Desensitizing it if there's no NEdit selection
			   disables this feature. */
			XtSetSensitive(window->filterItem, selected);

			DimSelectionDepUserMenuItems(window, selected);
			if (window->replaceDlog != nullptr && XtIsManaged(window->replaceDlog)) {
				UpdateReplaceActionButtons(window);
			}
		}
	}

	/* When the program needs to make a change to a text area without without
	   recording it for undo or marking file as changed it sets ignoreModify */
	if (window->ignoreModify || (nDeleted == 0 && nInserted == 0))
		return;

	/* Make sure line number display is sufficient for new data */
	window->updateLineNumDisp();

	/* Save information for undoing this operation (this call also counts
	   characters and editing operations for triggering autosave */
	window->SaveUndoInformation(pos, nInserted, nDeleted, deletedText);

	/* Trigger automatic backup if operation or character limits reached */
	if (window->autoSave && (window->autoSaveCharCount > AUTOSAVE_CHAR_LIMIT || window->autoSaveOpCount > AUTOSAVE_OP_LIMIT)) {
		WriteBackupFile(window);
		window->autoSaveCharCount = 0;
		window->autoSaveOpCount = 0;
	}

	/* Indicate that the window has now been modified */
	window->SetWindowModified(TRUE);

	/* Update # of bytes, and line and col statistics */
	window->UpdateStatsLine();

	/* Check if external changes have been made to file and warn user */
	CheckForChangesToFile(window);
}

void focusCB(Widget w, XtPointer clientData, XtPointer callData) {

	auto window = static_cast<Document *>(clientData);

	(void)callData;

	/* record which window pane last had the keyboard focus */
	window->lastFocus = w;

	/* update line number statistic to reflect current focus pane */
	window->UpdateStatsLine();

	/* finish off the current incremental search */
	EndISearch(window);

	/* Check for changes to read-only status and/or file modifications */
	CheckForChangesToFile(window);
}

void movedCB(Widget w, XtPointer clientData, XtPointer callData) {

	auto window = static_cast<Document *>(clientData);

	(void)callData;

	TextWidget textWidget = (TextWidget)w;

	if (window->ignoreModify)
		return;

	/* update line and column nubers in statistics line */
	window->UpdateStatsLine();

	/* Check the character before the cursor for matchable characters */
	FlashMatching(window, w);

	/* Check for changes to read-only status and/or file modifications */
	CheckForChangesToFile(window);

	/*  This callback is not only called for focussed panes, but for newly
	    created panes as well. So make sure that the cursor is left alone
	    for unfocussed panes.
	    TextWidget have no state per se about focus, so we use the related
	    ID for the blink procedure.  */
	if (textWidget->text.cursorBlinkProcID != 0) {
		/*  Start blinking the caret again.  */
		ResetCursorBlink(textWidget, False);
	}
}

void dragStartCB(Widget w, XtPointer clientData, XtPointer callData) {

	auto window = static_cast<Document *>(clientData);

	(void)callData;
	(void)w;

	/* don't record all of the intermediate drag steps for undo */
	window->ignoreModify = True;
}

void dragEndCB(Widget w, XtPointer clientData, XtPointer call_data) {

	(void)w;
	
	auto window   = static_cast<Document *>(clientData);
	auto callData = static_cast<dragEndCBStruct *>(call_data);

	/* restore recording of undo information */
	window->ignoreModify = False;

	/* Do nothing if drag operation was canceled */
	if (callData->nCharsInserted == 0)
		return;

	/* Save information for undoing this operation not saved while
	   undo recording was off */
	modifiedCB(callData->startPos, callData->nCharsInserted, callData->nCharsDeleted, 0, callData->deletedText, window);
}

Widget createTextArea(Widget parent, Document *window, int rows, int cols, int emTabDist, char *delimiters, int wrapMargin, int lineNumCols) {

	/* Create a text widget inside of a scrolled window widget */
	Widget sw         = XtVaCreateManagedWidget("scrolledW", xmScrolledWindowWidgetClass, parent, XmNpaneMaximum, SHRT_MAX, XmNpaneMinimum, PANE_MIN_HEIGHT, XmNhighlightThickness, 0, nullptr);
	Widget hScrollBar = XtVaCreateManagedWidget("textHorScrollBar", xmScrollBarWidgetClass, sw, XmNorientation, XmHORIZONTAL, XmNrepeatDelay, 10, nullptr);
	Widget vScrollBar = XtVaCreateManagedWidget("textVertScrollBar", xmScrollBarWidgetClass, sw, XmNorientation, XmVERTICAL, XmNrepeatDelay, 10, nullptr);
	Widget frame      = XtVaCreateManagedWidget("textFrame", xmFrameWidgetClass, sw, XmNshadowType, XmSHADOW_IN, nullptr);
	
	Widget text = XtVaCreateManagedWidget(
		"text", 
		textWidgetClass, 
		frame, 
		textNbacklightCharTypes, 
		window->backlightCharTypes, 
		textNrows, 
		rows, 
		textNcolumns, 
		cols, 
		textNlineNumCols, 
		lineNumCols, 
		textNemulateTabs, 
		emTabDist, 
		textNfont,
	    GetDefaultFontStruct(window->fontList), 
		textNhScrollBar, 
		hScrollBar, 
		textNvScrollBar, 
		vScrollBar, 
		textNreadOnly, 
		IS_ANY_LOCKED(window->lockReasons), 
		textNwordDelimiters, 
		delimiters, 
		textNwrapMargin,
	    wrapMargin, 
		textNautoIndent, 
		window->indentStyle == AUTO_INDENT, 
		textNsmartIndent, 
		window->indentStyle == SMART_INDENT, 
		textNautoWrap, 
		window->wrapMode == NEWLINE_WRAP, 
		textNcontinuousWrap,
	    window->wrapMode == CONTINUOUS_WRAP, 
		textNoverstrike, 
		window->overstrike, 
		textNhidePointer, 
		(Boolean)GetPrefTypingHidesPointer(), 
		textNcursorVPadding, 
		GetVerticalAutoScroll(), 
		nullptr);

	XtVaSetValues(sw, XmNworkWindow, frame, XmNhorizontalScrollBar, hScrollBar, XmNverticalScrollBar, vScrollBar, nullptr);

	/* add focus, drag, cursor tracking, and smart indent callbacks */
	XtAddCallback(text, textNfocusCallback, focusCB, window);
	XtAddCallback(text, textNcursorMovementCallback, movedCB, window);
	XtAddCallback(text, textNdragStartCallback, dragStartCB, window);
	XtAddCallback(text, textNdragEndCallback, dragEndCB, window);
	XtAddCallback(text, textNsmartIndentCallback, SmartIndentCB, window);

	/* This makes sure the text area initially has a the insert point shown
	   ... (check if still true with the nedit text widget, probably not) */
	XmAddTabGroup(containingPane(text));

	/* compensate for Motif delete/backspace problem */
	RemapDeleteKey(text);

	/* Augment translation table for right button popup menu */
	AddBGMenuAction(text);

	/* If absolute line numbers will be needed for display in the statistics
	   line, tell the widget to maintain them (otherwise, it's a costly
	   operation and performance will be better without it) */
	reinterpret_cast<TextWidget>(text)->text.textD->TextDMaintainAbsLineNum(window->showStats);

	return text;
}

/*
** hide all the tearoffs spawned from this menu.
** It works recursively to close the tearoffs of the submenus
*/
void hideTearOffs(Widget menuPane) {
	WidgetList itemList;
	Widget subMenuID;
	Cardinal nItems;
	int n;

	/* hide all submenu tearoffs */
	XtVaGetValues(menuPane, XmNchildren, &itemList, XmNnumChildren, &nItems, nullptr);
	for (n = 0; n < (int)nItems; n++) {
		if (XtClass(itemList[n]) == xmCascadeButtonWidgetClass) {
			XtVaGetValues(itemList[n], XmNsubMenuId, &subMenuID, nullptr);
			hideTearOffs(subMenuID);
		}
	}

	/* hide tearoff for this menu */
	if (!XmIsMenuShell(XtParent(menuPane)))
		XtUnmapWidget(XtParent(menuPane));
}

/*
** Redisplay menu tearoffs previously hid by hideTearOffs()
*/
void redisplayTearOffs(Widget menuPane) {
	WidgetList itemList;
	Widget subMenuID;
	Cardinal nItems;
	int n;

	/* redisplay all submenu tearoffs */
	XtVaGetValues(menuPane, XmNchildren, &itemList, XmNnumChildren, &nItems, nullptr);
	for (n = 0; n < (int)nItems; n++) {
		if (XtClass(itemList[n]) == xmCascadeButtonWidgetClass) {
			XtVaGetValues(itemList[n], XmNsubMenuId, &subMenuID, nullptr);
			redisplayTearOffs(subMenuID);
		}
	}

	/* redisplay tearoff for this menu */
	if (!XmIsMenuShell(XtParent(menuPane)))
		ShowHiddenTearOff(menuPane);
}

/*
** return the integer position of a tab in the tabbar it
** belongs to, or -1 if there's an error, somehow.
*/
int getTabPosition(Widget tab) {
	WidgetList tabList;
	int tabCount;
	Widget tabBar = XtParent(tab);

	XtVaGetValues(tabBar, XmNtabWidgetList, &tabList, XmNtabCount, &tabCount, nullptr);

	for (int i = 0; i < tabCount; i++) {
		if (tab == tabList[i]) {
			return i;
		}
	}

	return -1; /* something is wrong! */
}

std::list<UndoInfo *> cloneUndoItems(const std::list<UndoInfo *> &orgList) {

	std::list<UndoInfo *> list;
	for(UndoInfo *undo : orgList) {
		list.push_back(new UndoInfo(*undo));
	}
	return list;
}

void cloneTextPanes(Document *window, Document *orgWin) {
	short paneHeights[MAX_PANES + 1];
	int insertPositions[MAX_PANES + 1], topLines[MAX_PANES + 1];
	int horizOffsets[MAX_PANES + 1];
	int i, focusPane, emTabDist, wrapMargin, lineNumCols, totalHeight = 0;
	char *delimiters;
	Widget text;
	TextSelection sel;
	textDisp *textD, *newTextD;

	/* transfer the primary selection */
	memcpy(&sel, &orgWin->buffer->primary_, sizeof(TextSelection));

	if (sel.selected) {
		if (sel.rectangular)
			window->buffer->BufRectSelect(sel.start, sel.end, sel.rectStart, sel.rectEnd);
		else
			window->buffer->BufSelect(sel.start, sel.end);
	} else
		window->buffer->BufUnselect();

	/* Record the current heights, scroll positions, and insert positions
	   of the existing panes, keyboard focus */
	focusPane = 0;
	for (i = 0; i <= orgWin->nPanes; i++) {
		text = i == 0 ? orgWin->textArea : orgWin->textPanes[i - 1];
		insertPositions[i] = TextGetCursorPos(text);
		XtVaGetValues(containingPane(text), XmNheight, &paneHeights[i], nullptr);
		totalHeight += paneHeights[i];
		TextGetScroll(text, &topLines[i], &horizOffsets[i]);
		if (text == orgWin->lastFocus)
			focusPane = i;
	}

	window->nPanes = orgWin->nPanes;

	/* Copy some parameters */
	XtVaGetValues(orgWin->textArea, textNemulateTabs, &emTabDist, textNwordDelimiters, &delimiters, textNwrapMargin, &wrapMargin, nullptr);
	lineNumCols = orgWin->showLineNumbers ? MIN_LINE_NUM_COLS : 0;
	XtVaSetValues(window->textArea, textNemulateTabs, emTabDist, textNwordDelimiters, delimiters, textNwrapMargin, wrapMargin, textNlineNumCols, lineNumCols, nullptr);

	/* clone split panes, if any */
	textD = ((TextWidget)window->textArea)->text.textD;
	if (window->nPanes) {
		/* Unmanage & remanage the panedWindow so it recalculates pane heights */
		XtUnmanageChild(window->splitPane);

		/* Create a text widget to add to the pane and set its buffer and
		   highlight data to be the same as the other panes in the orgWin */

		for (i = 0; i < orgWin->nPanes; i++) {
			text = createTextArea(window->splitPane, window, 1, 1, emTabDist, delimiters, wrapMargin, lineNumCols);
			TextSetBuffer(text, window->buffer);

			if (window->highlightData)
				AttachHighlightToWidget(text, window);
			XtManageChild(text);
			window->textPanes[i] = text;

			/* Fix up the colors */
			newTextD = reinterpret_cast<TextWidget>(text)->text.textD;
			XtVaSetValues(text, XmNforeground, textD->fgPixel, XmNbackground, textD->bgPixel, nullptr);
			newTextD->TextDSetColors(textD->fgPixel, textD->bgPixel, textD->selectFGPixel, textD->selectBGPixel, textD->highlightFGPixel, textD->highlightBGPixel, textD->lineNumFGPixel, textD->cursorFGPixel);
		}

		/* Set the minimum pane height in the new pane */
		window->UpdateMinPaneHeights();

		for (i = 0; i <= window->nPanes; i++) {
			text = i == 0 ? window->textArea : window->textPanes[i - 1];
			setPaneDesiredHeight(containingPane(text), paneHeights[i]);
		}

		/* Re-manage panedWindow to recalculate pane heights & reset selection */
		XtManageChild(window->splitPane);
	}

	/* Reset all of the heights, scroll positions, etc. */
	for (i = 0; i <= window->nPanes; i++) {
		text = (i == 0) ? window->textArea : window->textPanes[i - 1];
		TextSetCursorPos(text, insertPositions[i]);
		TextSetScroll(text, topLines[i], horizOffsets[i]);

		/* dim the cursor */
		auto textD = reinterpret_cast<TextWidget>(text)->text.textD;
		textD->TextDSetCursorStyle(DIM_CURSOR);
		textD->TextDUnblankCursor();
	}

	/* set the focus pane */
	// NOTE(eteran): are we sure we want "<=" here? It's of course possible that
	//               it's correct, but it is certainly unconventional.
	//               Notice that is is used in the above loops as well
	for (i = 0; i <= window->nPanes; i++) {
		text = i == 0 ? window->textArea : window->textPanes[i - 1];
		if (i == focusPane) {
			window->lastFocus = text;
			XmProcessTraversal(text, XmTRAVERSE_CURRENT);
			break;
		}
	}

	/* Update the window manager size hints after the sizes of the panes have
	   been set (the widget heights are not yet readable here, but they will
	   be by the time the event loop gets around to running this timer proc) */
	XtAppAddTimeOut(XtWidgetToApplicationContext(window->shell), 0, wmSizeUpdateProc, window);
}

/* Add an icon to an applicaction shell widget.  addWindowIcon adds a large
** (primary window) icon, AddSmallIcon adds a small (secondary window) icon.
**
** Note: I would prefer that these were not hardwired, but yhere is something
** weird about the  XmNiconPixmap resource that prevents it from being set
** from the defaults in the application resource database.
*/
void addWindowIcon(Widget shell) {
	static Pixmap iconPixmap = 0, maskPixmap = 0;

	if (iconPixmap == 0) {
		iconPixmap = XCreateBitmapFromData(TheDisplay, RootWindowOfScreen(XtScreen(shell)), (char *)iconBits, iconBitmapWidth, iconBitmapHeight);
		maskPixmap = XCreateBitmapFromData(TheDisplay, RootWindowOfScreen(XtScreen(shell)), (char *)maskBits, iconBitmapWidth, iconBitmapHeight);
	}
	XtVaSetValues(shell, XmNiconPixmap, iconPixmap, XmNiconMask, maskPixmap, nullptr);
}

void hideTooltip(Widget tab) {
	if (Widget tooltip = XtNameToWidget(tab, "*BubbleShell"))
		XtPopdown(tooltip);
}

/*
** ButtonPress event handler for tabs.
*/
void tabClickEH(Widget w, XtPointer clientData, XEvent *event) {

	(void)clientData;
	(void)event;

	/* hide the tooltip when user clicks with any button. */
	if (BubbleButton_Timer(w)) {
		XtRemoveTimeOut(BubbleButton_Timer(w));
		BubbleButton_Timer(w) = (XtIntervalId) nullptr;
	} else {
		hideTooltip(w);
	}
}

/*
** add a tab to the tab bar for the new document.
*/
Widget addTab(Widget folder, const char *string) {
	Widget tooltipLabel, tab;
	XmString s1;

	s1 = XmStringCreateSimpleEx(string);
	tab = XtVaCreateManagedWidget("tab", xrwsBubbleButtonWidgetClass, folder,
	                              /* XmNmarginWidth, <default@nedit.c>, */
	                              /* XmNmarginHeight, <default@nedit.c>, */
	                              /* XmNalignment, <default@nedit.c>, */
	                              XmNlabelString, s1, XltNbubbleString, s1, XltNshowBubble, GetPrefToolTips(), XltNautoParkBubble, True, XltNslidingBubble, False,
	                              /* XltNdelay, 800,*/
	                              /* XltNbubbleDuration, 8000,*/
	                              nullptr);
	XmStringFree(s1);

	/* there's things to do as user click on the tab */
	XtAddEventHandler(tab, ButtonPressMask, False, (XtEventHandler)tabClickEH, nullptr);

	/* BubbleButton simply use reversed video for tooltips,
	   we try to use the 'standard' color */
	tooltipLabel = XtNameToWidget(tab, "*BubbleLabel");
	XtVaSetValues(tooltipLabel, XmNbackground, AllocateColor(tab, GetPrefTooltipBgColor()), XmNforeground, AllocateColor(tab, NEDIT_DEFAULT_FG), nullptr);

	/* put borders around tooltip. BubbleButton use
	   transientShellWidgetClass as tooltip shell, which
	   came without borders */
	XtVaSetValues(XtParent(tooltipLabel), XmNborderWidth, 1, nullptr);

	return tab;
}

/*
** Create pixmap per the widget's color depth setting.
**
** This fixes a BadMatch (X_CopyArea) error due to mismatching of
** color depth between the bitmap (depth of 1) and the screen,
** specifically on when linked to LessTif v1.2 (release 0.93.18
** & 0.93.94 tested).  LessTif v2.x showed no such problem.
*/
Pixmap createBitmapWithDepth(Widget w, char *data, unsigned int width, unsigned int height) {
	Pixmap pixmap;
	Pixel fg, bg;
	int depth;

	XtVaGetValues(w, XmNforeground, &fg, XmNbackground, &bg, XmNdepth, &depth, nullptr);
	pixmap = XCreatePixmapFromBitmapData(XtDisplay(w), RootWindowOfScreen(XtScreen(w)), (char *)data, width, height, fg, bg, depth);

	return pixmap;
}

void closeTabProc(XtPointer clientData, XtIntervalId *id) {
	(void)id;
	CloseFileAndWindow(static_cast<Document *>(clientData), PROMPT_SBC_DIALOG_RESPONSE);
}

void CloseDocumentWindow(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	
	auto window = static_cast<Document *>(clientData);

	int nDocuments = window->NDocuments();

	if (nDocuments == NWindows()) {
		/* this is only window, then exit */
		XtCallActionProc(WindowList->lastFocus, "exit", ((XmAnyCallbackStruct *)callData)->event, nullptr, 0);
	} else {
		if (nDocuments == 1) {
			CloseFileAndWindow(window, PROMPT_SBC_DIALOG_RESPONSE);
		} else {
			int resp = 1;
			if (GetPrefWarnExit())
				resp = DialogF(DF_QUES, window->shell, 2, "Close Window", "Close ALL documents in this window?", "Close", "Cancel");

			if (resp == 1)
				window->CloseAllDocumentInWindow();
		}
	}
}


void closeCB(Widget w, XtPointer clientData, XtPointer callData) {

	auto window = static_cast<Document *>(clientData);
	
	window = WidgetToWindow(w);
	if (!WindowCanBeClosed(window)) {
		return;
	}

	CloseDocumentWindow(w, window, callData);
}

/*
** callback to close-tab button.
*/
void closeTabCB(Widget w, XtPointer clientData, XtPointer callData) {

	auto mainWin = static_cast<Widget>(clientData);

	(void)callData;

	/* FIXME: XtRemoveActionHook() related coredump

	   An unknown bug seems to be associated with the XtRemoveActionHook()
	   call in FinishLearn(), which resulted in coredump if a tab was
	   closed, in the middle of keystrokes learning, by clicking on the
	   close-tab button.

	   As evident to our accusation, the coredump may be surpressed by
	   simply commenting out the XtRemoveActionHook() call. The bug was
	   consistent on both Motif and Lesstif on various platforms.

	   Closing the tab through either the "Close" menu or its accel key,
	   however, was without any trouble.

	   While its actual mechanism is not well understood, we somehow
	   managed to workaround the bug by delaying the action of closing
	   the tab. For now. */
	XtAppAddTimeOut(XtWidgetToApplicationContext(w), 0, closeTabProc, GetTopDocument(mainWin));
}

/*
** callback to clicks on a tab to raise it's document.
*/
void raiseTabCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)clientData;

	XmLFolderCallbackStruct *cbs = (XmLFolderCallbackStruct *)callData;
	WidgetList tabList;
	Widget tab;

	XtVaGetValues(w, XmNtabWidgetList, &tabList, nullptr);
	tab = tabList[cbs->pos];
	TabToWindow(tab)->RaiseDocument();
}

}

/*
** set/clear menu sensitivity if the calling document is on top.
*/
void Document::SetSensitive(Widget w, Boolean sensitive) {
	if (this->IsTopDocument()) {
		XtSetSensitive(w, sensitive);
	}
}

/*
** set/clear toggle menu state if the calling document is on top.
*/
void Document::SetToggleButtonState(Widget w, Boolean state, Boolean notify) {
	if (this->IsTopDocument()) {
		XmToggleButtonSetState(w, state, notify);
	}
}

/*
** remember the last document.
*/
Document *Document::MarkLastDocument() {
	Document *prev = lastFocusDocument;

	if (this)
		lastFocusDocument = this;

	return prev;
}

/*
** remember the active (top) document.
*/
Document *Document::MarkActiveDocument() {
	Document *prev = inFocusDocument;

	if (this)
		inFocusDocument = this;

	return prev;
}

/*
** Bring up the last active window
*/
void Document::LastDocument() {
	Document *win;

	for (win = WindowList; win; win = win->next) {
		if (lastFocusDocument == win) {
			break;
		}
	}

	if (!win) {
		return;
	}

	if (this->shell == win->shell) {
		win->RaiseDocument();
	} else {
		win->RaiseFocusDocumentWindow(True);
	}
}

/*
** raise the document and its shell window and optionally focus.
*/
void Document::RaiseFocusDocumentWindow(Boolean focus) {
	if (!this)
		return;

	this->RaiseDocument();
	RaiseShellWindow(this->shell, focus);
}

/*
** 
*/
bool Document::IsTopDocument() const {
	return this == GetTopDocument(this->shell);
}

/*
** 
*/
Widget Document::GetPaneByIndex(int paneIndex) const {
	Widget text = nullptr;
	if (paneIndex >= 0 && paneIndex <= this->nPanes) {
		text = (paneIndex == 0) ? this->textArea : this->textPanes[paneIndex - 1];
	}
	return text;
}

/*
** make sure window is alive is kicking
*/
int Document::IsValidWindow() {

	for (Document *win = WindowList; win; win = win->next) {
		if (this == win) {
			return true;
		}
	}

	return false;
}

/*
** check if tab bar is to be shown on this window
*/
int Document::GetShowTabBar() {
	if (!GetPrefTabBar())
		return False;
	else if (this->NDocuments() == 1)
		return !GetPrefTabBarHideOne();
	else
		return True;
}

/*
** Returns true if window is iconic (as determined by the WM_STATE property
** on the shell window.  I think this is the most reliable way to tell,
** but if someone has a better idea please send me a note).
*/
int Document::IsIconic() {
	unsigned long *property = nullptr;
	unsigned long nItems;
	unsigned long leftover;
	static Atom wmStateAtom = 0;
	Atom actualType;
	int actualFormat;
	
	if (wmStateAtom == 0) {
		wmStateAtom = XInternAtom(XtDisplay(this->shell), "WM_STATE", False);
	}
		
	if (XGetWindowProperty(XtDisplay(this->shell), XtWindow(this->shell), wmStateAtom, 0L, 1L, False, wmStateAtom, &actualType, &actualFormat, &nItems, &leftover, (unsigned char **)&property) != Success || nItems != 1 || property == nullptr) {
		return FALSE;
	}

	int result = (*property == IconicState);
	XtFree((char *)property);
	return result;
}

/*
** return the next/previous docment on the tab list.
**
** If <wrap> is true then the next tab of the rightmost tab will be the
** second tab from the right, and the the previous tab of the leftmost
** tab will be the second from the left.  This is useful for getting
** the next tab after a tab detaches/closes and you don't want to wrap around.
*/
Document *Document::getNextTabWindow(int direction, int crossWin, int wrap) {
	WidgetList tabList;
	Document *win;
	int tabCount, tabTotalCount;
	int tabPos, nextPos;
	int i, n;
	int nBuf = crossWin ? NWindows() : this->NDocuments();

	if (nBuf <= 1)
		return nullptr;

	/* get the list of tabs */
	auto tabs = new Widget[nBuf];
	tabTotalCount = 0;
	if (crossWin) {
		int n, nItems;
		WidgetList children;

		XtVaGetValues(TheAppShell, XmNchildren, &children, XmNnumChildren, &nItems, nullptr);

		/* get list of tabs in all windows */
		for (n = 0; n < nItems; n++) {
			if (strcmp(XtName(children[n]), "textShell") || ((win = WidgetToWindow(children[n])) == nullptr))
				continue; /* skip non-text-editor windows */

			XtVaGetValues(win->tabBar, XmNtabWidgetList, &tabList, XmNtabCount, &tabCount, nullptr);

			for (i = 0; i < tabCount; i++) {
				tabs[tabTotalCount++] = tabList[i];
			}
		}
	} else {
		/* get list of tabs in this window */
		XtVaGetValues(this->tabBar, XmNtabWidgetList, &tabList, XmNtabCount, &tabCount, nullptr);

		for (i = 0; i < tabCount; i++) {
			if (TabToWindow(tabList[i])) /* make sure tab is valid */
				tabs[tabTotalCount++] = tabList[i];
		}
	}

	/* find the position of the tab in the tablist */
	tabPos = 0;
	for (n = 0; n < tabTotalCount; n++) {
		if (tabs[n] == this->tab) {
			tabPos = n;
			break;
		}
	}

	/* calculate index position of next tab */
	nextPos = tabPos + direction;
	if (nextPos >= nBuf) {
		if (wrap)
			nextPos = 0;
		else
			nextPos = nBuf - 2;
	} else if (nextPos < 0) {
		if (wrap)
			nextPos = nBuf - 1;
		else
			nextPos = 1;
	}

	/* return the document where the next tab belongs to */
	win = TabToWindow(tabs[nextPos]);
	delete [] tabs;
	return win;
}

/*
** Displays and undisplays the statistics line (regardless of settings of
** this->showStats or this->modeMessageDisplayed)
*/
void Document::showStatistics(int state) {
	if (state) {
		XtManageChild(this->statsLineForm);
		this->showStatsForm();
	} else {
		XtUnmanageChild(this->statsLineForm);
		this->showStatsForm();
	}

	/* Tell WM that the non-expandable part of the this has changed size */
	/* Already done in showStatsForm */
	/* this->UpdateWMSizeHints(); */
}

/*
** Put up or pop-down the incremental search line regardless of settings
** of showISearchLine or TempShowISearch
*/
void Document::showISearch(int state) {
	if (state) {
		XtManageChild(this->iSearchForm);
		this->showStatsForm();
	} else {
		XtUnmanageChild(this->iSearchForm);
		this->showStatsForm();
	}

	/* Tell WM that the non-expandable part of the this has changed size */
	/* This is already done in showStatsForm */
	/* this->UpdateWMSizeHints(); */
}

/*
** Show or hide the extra display area under the main menu bar which
** optionally contains the status line and the incremental search bar
*/
void Document::showStatsForm() {
	Widget statsAreaForm = XtParent(this->statsLineForm);
	Widget mainW = XtParent(statsAreaForm);

	/* The very silly use of XmNcommandWindowLocation and XmNshowSeparator
	   below are to kick the main this widget to position and remove the
	   status line when it is managed and unmanaged.  At some Motif version
	   level, the showSeparator trick backfires and leaves the separator
	   shown, but fortunately the dynamic behavior is fixed, too so the
	   workaround is no longer necessary, either.  (... the version where
	   this occurs may be earlier than 2.1.  If the stats line shows
	   double thickness shadows in earlier Motif versions, the #if XmVersion
	   directive should be moved back to that earlier version) */
	if (manageToolBars(statsAreaForm)) {
		XtUnmanageChild(statsAreaForm); /*... will this fix Solaris 7??? */
		XtVaSetValues(mainW, XmNcommandWindowLocation, XmCOMMAND_ABOVE_WORKSPACE, nullptr);
#if XmVersion < 2001
		XtVaSetValues(mainW, XmNshowSeparator, True, nullptr);
#endif
		XtManageChild(statsAreaForm);
		XtVaSetValues(mainW, XmNshowSeparator, False, nullptr);
		this->UpdateStatsLine();
	} else {
		XtUnmanageChild(statsAreaForm);
		XtVaSetValues(mainW, XmNcommandWindowLocation, XmCOMMAND_BELOW_WORKSPACE, nullptr);
	}

	/* Tell WM that the non-expandable part of the this has changed size */
	this->UpdateWMSizeHints();
}

/*
** Add a this to the the this list.
*/
void Document::addToWindowList() {

	Document *temp = WindowList;
	WindowList = this;
	this->next = temp;
}

/*
** Remove a this from the list of windows
*/
void Document::removeFromWindowList() {
	Document *temp;

	if (WindowList == this) {
		WindowList = this->next;
	} else {
		for (temp = WindowList; temp != nullptr; temp = temp->next) {
			if (temp->next == this) {
				temp->next = this->next;
				break;
			}
		}
	}
}

/*
** Remove redundant expose events on tab bar.
*/
void Document::CleanUpTabBarExposeQueue() {
	XEvent event;
	XExposeEvent ev;
	int count;

	if(!this)
		return;

	/* remove redundant expose events on tab bar */
	count = 0;
	while (XCheckTypedWindowEvent(TheDisplay, XtWindow(this->tabBar), Expose, &event))
		count++;

	/* now we can update tabbar */
	if (count) {
		ev.type = Expose;
		ev.display = TheDisplay;
		ev.window = XtWindow(this->tabBar);
		ev.x = 0;
		ev.y = 0;
		ev.width = XtWidth(this->tabBar);
		ev.height = XtHeight(this->tabBar);
		ev.count = 0;
		XSendEvent(TheDisplay, XtWindow(this->tabBar), False, ExposureMask, (XEvent *)&ev);
	}
}

/*
** refresh window state for this document
*/
void Document::RefreshWindowStates() {
	if (!this->IsTopDocument())
		return;

	if (this->modeMessageDisplayed)
		XmTextSetStringEx(this->statsLine, this->modeMessage);
	else
		this->UpdateStatsLine();
		
	this->UpdateWindowReadOnly();
	this->UpdateWindowTitle();

	/* show/hide statsline as needed */
	if (this->modeMessageDisplayed && !XtIsManaged(this->statsLineForm)) {
		/* turn on statline to display mode message */
		this->showStatistics(True);
	} else if (this->showStats && !XtIsManaged(this->statsLineForm)) {
		/* turn on statsline since it is enabled */
		this->showStatistics(True);
	} else if (!this->showStats && !this->modeMessageDisplayed && XtIsManaged(this->statsLineForm)) {
		/* turn off statsline since there's nothing to show */
		this->showStatistics(False);
	}

	/* signal if macro/shell is running */
	if (this->shellCmdData || this->macroCmdData)
		BeginWait(this->shell);
	else
		EndWait(this->shell);

	/* we need to force the statsline to reveal itself */
	if (XtIsManaged(this->statsLineForm)) {
		XmTextSetCursorPosition(this->statsLine, 0);    /* start of line */
		XmTextSetCursorPosition(this->statsLine, 9000); /* end of line */
	}

	XmUpdateDisplay(this->statsLine);
	this->refreshMenuBar();

	this->updateLineNumDisp();
}

/*
** Refresh the various settings/state of the shell window per the
** settings of the top document.
*/
void Document::refreshMenuBar() {
	this->RefreshMenuToggleStates();

	/* Add/remove language specific menu items */
	UpdateUserMenus(this);

	/* refresh selection-sensitive menus */
	DimSelectionDepUserMenuItems(this, this->wasSelected);
}

/*
**  If necessary, enlarges the window and line number display area to make
**  room for numbers.
*/
int Document::updateLineNumDisp() {
	if (!this->showLineNumbers) {
		return 0;
	}

	/* Decide how wide the line number field has to be to display all
	   possible line numbers */
	return this->updateGutterWidth();
}

/*
**  Set the new gutter width in the window. Sadly, the only way to do this is
**  to set it on every single document, so we have to iterate over them.
**
**  (Iteration taken from NDocuments(); is there a better way to do it?)
*/
int Document::updateGutterWidth() {
	Document *document;
	int reqCols = MIN_LINE_NUM_COLS;
	int newColsDiff = 0;
	int maxCols = 0;

	for (document = WindowList; nullptr != document; document = document->next) {
		if (document->shell == this->shell) {
			/*  We found ourselves a document from this this.  */
			int lineNumCols, tmpReqCols;
			textDisp *textD = ((TextWidget)document->textArea)->text.textD;

			XtVaGetValues(document->textArea, textNlineNumCols, &lineNumCols, nullptr);

			/* Is the width of the line number area sufficient to display all the
			   line numbers in the file?  If not, expand line number field, and the
			   this width. */

			if (lineNumCols > maxCols) {
				maxCols = lineNumCols;
			}

			tmpReqCols = textD->nBufferLines < 1 ? 1 : (int)log10((double)textD->nBufferLines + 1) + 1;

			if (tmpReqCols > reqCols) {
				reqCols = tmpReqCols;
			}
		}
	}

	if (reqCols != maxCols) {
		XFontStruct *fs;
		Dimension windowWidth;
		short fontWidth;

		newColsDiff = reqCols - maxCols;

		XtVaGetValues(this->textArea, textNfont, &fs, nullptr);
		fontWidth = fs->max_bounds.width;

		XtVaGetValues(this->shell, XmNwidth, &windowWidth, nullptr);
		XtVaSetValues(this->shell, XmNwidth, (Dimension)windowWidth + (newColsDiff * fontWidth), nullptr);

		this->UpdateWMSizeHints();
	}

	for (document = WindowList; nullptr != document; document = document->next) {
		if (document->shell == this->shell) {
			Widget text;
			int i;
			int lineNumCols;

			XtVaGetValues(document->textArea, textNlineNumCols, &lineNumCols, nullptr);

			if (lineNumCols == reqCols) {
				continue;
			}

			/*  Update all panes of this document.  */
			for (i = 0; i <= document->nPanes; i++) {
				text = (i == 0) ? document->textArea : document->textPanes[i - 1];
				XtVaSetValues(text, textNlineNumCols, reqCols, nullptr);
			}
		}
	}

	return reqCols;
}

/*
** Refresh the menu entries per the settings of the
** top document.
*/
void Document::RefreshMenuToggleStates() {

	if (!this->IsTopDocument())
		return;

	/* File menu */
	XtSetSensitive(this->printSelItem, this->wasSelected);

	/* Edit menu */
	XtSetSensitive(this->undoItem, !this->undo.empty());
	XtSetSensitive(this->redoItem, !this->redo.empty());
	XtSetSensitive(this->printSelItem, this->wasSelected);
	XtSetSensitive(this->cutItem, this->wasSelected);
	XtSetSensitive(this->copyItem, this->wasSelected);
	XtSetSensitive(this->delItem, this->wasSelected);

	/* Preferences menu */
	XmToggleButtonSetState(this->statsLineItem, this->showStats, False);
	XmToggleButtonSetState(this->iSearchLineItem, this->showISearchLine, False);
	XmToggleButtonSetState(this->lineNumsItem, this->showLineNumbers, False);
	XmToggleButtonSetState(this->highlightItem, this->highlightSyntax, False);
	XtSetSensitive(this->highlightItem, this->languageMode != PLAIN_LANGUAGE_MODE);
	XmToggleButtonSetState(this->backlightCharsItem, this->backlightChars, False);
	XmToggleButtonSetState(this->saveLastItem, this->saveOldVersion, False);
	XmToggleButtonSetState(this->autoSaveItem, this->autoSave, False);
	XmToggleButtonSetState(this->overtypeModeItem, this->overstrike, False);
	XmToggleButtonSetState(this->matchSyntaxBasedItem, this->matchSyntaxBased, False);
	XmToggleButtonSetState(this->readOnlyItem, IS_USER_LOCKED(this->lockReasons), False);

	XtSetSensitive(this->smartIndentItem, SmartIndentMacrosAvailable(LanguageModeName(this->languageMode)));

	this->SetAutoIndent(this->indentStyle);
	this->SetAutoWrap(this->wrapMode);
	this->SetShowMatching(this->showMatchingStyle);
	SetLanguageMode(this, this->languageMode, FALSE);

	/* Windows Menu */
	XtSetSensitive(this->splitPaneItem, this->nPanes < MAX_PANES);
	XtSetSensitive(this->closePaneItem, this->nPanes > 0);
	XtSetSensitive(this->detachDocumentItem, this->NDocuments() > 1);
	XtSetSensitive(this->contextDetachDocumentItem, this->NDocuments() > 1);

	Document *win;
	for (win = WindowList; win; win = win->next)
		if (win->shell != this->shell)
			break;
	XtSetSensitive(this->moveDocumentItem, win != nullptr);
}

/*
** update the tab label, etc. of a tab, per the states of it's document.
*/
void Document::RefreshTabState() {
	XmString s1, tipString;
	char labelString[MAXPATHLEN];
	char *tag = XmFONTLIST_DEFAULT_TAG;
	unsigned char alignment;

	/* Set tab label to document's filename. Position of
	   "*" (modified) will change per label alignment setting */
	XtVaGetValues(this->tab, XmNalignment, &alignment, nullptr);
	if (alignment != XmALIGNMENT_END) {
		sprintf(labelString, "%s%s", this->fileChanged ? "*" : "", this->filename);
	} else {
		sprintf(labelString, "%s%s", this->filename, this->fileChanged ? "*" : "");
	}

	/* Make the top document stand out a little more */
	if (this->IsTopDocument())
		tag = (String) "BOLD";

	s1 = XmStringCreateLtoREx(labelString, tag);

	if (GetPrefShowPathInWindowsMenu() && this->filenameSet) {
		strcat(labelString, " - ");
		strcat(labelString, this->path);
	}
	tipString = XmStringCreateSimpleEx(labelString);

	XtVaSetValues(this->tab, XltNbubbleString, tipString, XmNlabelString, s1, nullptr);
	XmStringFree(s1);
	XmStringFree(tipString);
}

/*
** return the number of documents owned by this shell window
*/
int Document::NDocuments() {
	Document *win;
	int nDocument = 0;

	for (win = WindowList; win; win = win->next) {
		if (win->shell == this->shell)
			nDocument++;
	}

	return nDocument;
}

/*
** raise the document and its shell window and focus depending on pref.
*/
void Document::RaiseDocumentWindow() {
	if (!this)
		return;

	this->RaiseDocument();
	RaiseShellWindow(this->shell, GetPrefFocusOnRaise());
}

/*
** Update the optional statistics line.
*/
void Document::UpdateStatsLine() {
	int line;
	int colNum;	
	Widget statW = this->statsLine;
	XmString xmslinecol;

	if (!this->IsTopDocument()) {
		return;
	}

	/* This routine is called for each character typed, so its performance
	   affects overall editor perfomance.  Only update if the line is on. */
	if (!this->showStats) {
		return;
	}

	/* Compose the string to display. If line # isn't available, leave it off */
	int pos            = TextGetCursorPos(this->lastFocus);
	size_t string_size = strlen(this->filename) + strlen(this->path) + 45;
	auto string        = new char[string_size];
	const char *format = (this->fileFormat == DOS_FILE_FORMAT) ? " DOS" : (this->fileFormat == MAC_FILE_FORMAT ? " Mac" : "");
	char slinecol[32];
	
	if (!TextPosToLineAndCol(this->lastFocus, pos, &line, &colNum)) {
		snprintf(string, string_size, "%s%s%s %d bytes", this->path, this->filename, format, this->buffer->BufGetLength());
		snprintf(slinecol, sizeof(slinecol), "L: ---  C: ---");
	} else {
		snprintf(slinecol, sizeof(slinecol), "L: %d  C: %d", line, colNum);
		if (this->showLineNumbers) {
			snprintf(string, string_size, "%s%s%s byte %d of %d", this->path, this->filename, format, pos, this->buffer->BufGetLength());
		} else {
			snprintf(string, string_size, "%s%s%s %d bytes", this->path, this->filename, format, this->buffer->BufGetLength());
		}
	}

	/* Update the line/column number */
	xmslinecol = XmStringCreateSimpleEx(slinecol);
	XtVaSetValues(this->statsLineColNo, XmNlabelString, xmslinecol, nullptr);
	XmStringFree(xmslinecol);

	/* Don't clobber the line if there's a special message being displayed */
	if (!this->modeMessageDisplayed) {
		/* Change the text in the stats line */
		XmTextReplace(statW, 0, XmTextGetLastPosition(statW), string);
	}
	delete [] string;

	/* Update the line/col display */
	xmslinecol = XmStringCreateSimpleEx(slinecol);
	XtVaSetValues(this->statsLineColNo, XmNlabelString, xmslinecol, nullptr);
	XmStringFree(xmslinecol);
}

/*
** Update the window manager's size hints.  These tell it the increments in
** which it is allowed to resize the window.  While this isn't particularly
** important for NEdit (since it can tolerate any window size), setting these
** hints also makes the resize indicator show the window size in characters
** rather than pixels, which is very helpful to users.
*/
void Document::UpdateWMSizeHints() {
	Dimension shellWidth, shellHeight, textHeight, hScrollBarHeight;
	int marginHeight, marginWidth, totalHeight, nCols, nRows;
	XFontStruct *fs;
	int i, baseWidth, baseHeight, fontHeight, fontWidth;
	Widget hScrollBar;
	textDisp *textD = ((TextWidget)this->textArea)->text.textD;

	/* Find the dimensions of a single character of the text font */
	XtVaGetValues(this->textArea, textNfont, &fs, nullptr);
	fontHeight = textD->ascent + textD->descent;
	fontWidth = fs->max_bounds.width;

	/* Find the base (non-expandable) width and height of the editor this.

	   FIXME:
	   To workaround the shrinking-this bug on some WM such as Metacity,
	   which caused the this to shrink as we switch between documents
	   using different font sizes on the documents in the same this, the
	   base width, and similarly the base height, is ajusted such that:
	        shellWidth = baseWidth + cols * textWidth
	   There are two issues with this workaround:
	   1. the right most characters may appear partially obsure
	   2. the Col x Row info reported by the WM will be based on the fully
	      display text.
	*/
	XtVaGetValues(this->textArea, XmNheight, &textHeight, textNmarginHeight, &marginHeight, textNmarginWidth, &marginWidth, nullptr);
	totalHeight = textHeight - 2 * marginHeight;
	for (i = 0; i < this->nPanes; i++) {
		XtVaGetValues(this->textPanes[i], XmNheight, &textHeight, textNhScrollBar, &hScrollBar, nullptr);
		totalHeight += textHeight - 2 * marginHeight;
		if (!XtIsManaged(hScrollBar)) {
			XtVaGetValues(hScrollBar, XmNheight, &hScrollBarHeight, nullptr);
			totalHeight -= hScrollBarHeight;
		}
	}

	XtVaGetValues(this->shell, XmNwidth, &shellWidth, XmNheight, &shellHeight, nullptr);
	nCols = textD->width / fontWidth;
	nRows = totalHeight / fontHeight;
	baseWidth = shellWidth - nCols * fontWidth;
	baseHeight = shellHeight - nRows * fontHeight;

	/* Set the size hints in the shell widget */
	XtVaSetValues(this->shell, XmNwidthInc, fs->max_bounds.width, XmNheightInc, fontHeight, XmNbaseWidth, baseWidth, XmNbaseHeight, baseHeight, XmNminWidth, baseWidth + fontWidth, XmNminHeight,
	              baseHeight + (1 + this->nPanes) * fontHeight, nullptr);

	/* Motif will keep placing this on the shell every time we change it,
	   so it needs to be undone every single time.  This only seems to
	   happen on mult-head dispalys on screens 1 and higher. */

	RemovePPositionHint(this->shell);
}

/*
**
*/
int Document::WidgetToPaneIndex(Widget w) {
	int i;
	Widget text;
	int paneIndex = 0;

	for (i = 0; i <= this->nPanes; ++i) {
		text = (i == 0) ? this->textArea : this->textPanes[i - 1];
		if (text == w) {
			paneIndex = i;
		}
	}
	return (paneIndex);
}

/*
**
*/
void Document::SetTabDist(int tabDist) {
	if (this->buffer->tabDist_ != tabDist) {
		int saveCursorPositions[MAX_PANES + 1];
		int saveVScrollPositions[MAX_PANES + 1];
		int saveHScrollPositions[MAX_PANES + 1];
		int paneIndex;

		this->ignoreModify = True;

		for (paneIndex = 0; paneIndex <= this->nPanes; ++paneIndex) {
			Widget w = this->GetPaneByIndex(paneIndex);
			textDisp *textD = reinterpret_cast<TextWidget>(w)->text.textD;

			TextGetScroll(w, &saveVScrollPositions[paneIndex], &saveHScrollPositions[paneIndex]);
			saveCursorPositions[paneIndex] = TextGetCursorPos(w);
			textD->modifyingTabDist = 1;
		}

		this->buffer->BufSetTabDistance(tabDist);

		for (paneIndex = 0; paneIndex <= this->nPanes; ++paneIndex) {
			Widget w = this->GetPaneByIndex(paneIndex);
			textDisp *textD = reinterpret_cast<TextWidget>(w)->text.textD;

			textD->modifyingTabDist = 0;
			TextSetCursorPos(w, saveCursorPositions[paneIndex]);
			TextSetScroll(w, saveVScrollPositions[paneIndex], saveHScrollPositions[paneIndex]);
		}

		this->ignoreModify = False;
	}
}

/*
**
*/
void Document::SetEmTabDist(int emTabDist) {

	XtVaSetValues(this->textArea, textNemulateTabs, emTabDist, nullptr);
	for (int i = 0; i < this->nPanes; ++i) {
		XtVaSetValues(this->textPanes[i], textNemulateTabs, emTabDist, nullptr);
	}
}

/*
**
*/
static void showTabBar(Document *window, int state) {
	if (state) {
		XtManageChild(XtParent(window->tabBar));
		window->showStatsForm();
	} else {
		XtUnmanageChild(XtParent(window->tabBar));
		window->showStatsForm();
	}
}

/*
**
*/
void Document::ShowTabBar(int state) {
	if (XtIsManaged(XtParent(this->tabBar)) == state)
		return;
	showTabBar(this, state);
}

/*
** Turn on and off the continuing display of the incremental search line
** (when off, it is popped up and down as needed via TempShowISearch)
*/
void Document::ShowISearchLine(int state) {
	Document *win;

	if (this->showISearchLine == state)
		return;
	this->showISearchLine = state;
	this->showISearch(state);

	/* i-search line is shell-level, hence other tabbed
	   documents in the this should synch */
	for (win = WindowList; win; win = win->next) {
		if (win->shell != this->shell || win == this)
			continue;
		win->showISearchLine = state;
	}
}

/*
** Temporarily show and hide the incremental search line if the line is not
** already up.
*/
void Document::TempShowISearch(int state) {
	if (this->showISearchLine)
		return;
	if (XtIsManaged(this->iSearchForm) != state)
		this->showISearch(state);
}

/*
** Display a special message in the stats line (show the stats line if it
** is not currently shown).
*/
void Document::SetModeMessage(const char *message) {
	/* this document may be hidden (not on top) or later made hidden,
	   so we save a copy of the mode message, so we can restore the
	   statsline when the document is raised to top again */
	this->modeMessageDisplayed = True;
	XtFree(this->modeMessage);
	this->modeMessage = XtNewStringEx(message);

	if (!this->IsTopDocument())
		return;

	XmTextSetStringEx(this->statsLine, message);
	/*
	 * Don't invoke the stats line again, if stats line is already displayed.
	 */
	if (!this->showStats)
		this->showStatistics(True);
}

/*
** Clear special statistics line message set in SetModeMessage, returns
** the statistics line to its original state as set in window->showStats
*/
void Document::ClearModeMessage() {
	if (!this->modeMessageDisplayed)
		return;

	this->modeMessageDisplayed = False;
	XtFree(this->modeMessage);
	this->modeMessage = nullptr;

	if (!this->IsTopDocument())
		return;

	/*
	 * Remove the stats line only if indicated by it's window state.
	 */
	if (!this->showStats) {
		this->showStatistics(False);
	}
	
	this->UpdateStatsLine();
}

/*
** Set the backlight character class string
*/
void Document::SetBacklightChars(char *applyBacklightTypes) {
	int i;
	int is_applied = XmToggleButtonGetState(this->backlightCharsItem) ? 1 : 0;
	int do_apply = applyBacklightTypes ? 1 : 0;

	this->backlightChars = do_apply;

	XtFree(this->backlightCharTypes);
	
	
	if(this->backlightChars) {
		this->backlightCharTypes = XtStringDup(applyBacklightTypes);
	} else {
		this->backlightCharTypes = nullptr;
	}

	XtVaSetValues(this->textArea, textNbacklightCharTypes, this->backlightCharTypes, nullptr);
	for (i = 0; i < this->nPanes; i++)
		XtVaSetValues(this->textPanes[i], textNbacklightCharTypes, this->backlightCharTypes, nullptr);
	if (is_applied != do_apply)
		this->SetToggleButtonState(this->backlightCharsItem, do_apply, False);
}

/*
** Update the read-only state of the text area(s) in the window, and
** the ReadOnly toggle button in the File menu to agree with the state in
** the window data structure.
*/
void Document::UpdateWindowReadOnly() {
	int i, state;

	if (!this->IsTopDocument())
		return;

	state = IS_ANY_LOCKED(this->lockReasons);
	XtVaSetValues(this->textArea, textNreadOnly, state, nullptr);
	for (i = 0; i < this->nPanes; i++)
		XtVaSetValues(this->textPanes[i], textNreadOnly, state, nullptr);
	XmToggleButtonSetState(this->readOnlyItem, state, FALSE);
	XtSetSensitive(this->readOnlyItem, !IS_ANY_LOCKED_IGNORING_USER(this->lockReasons));
}

/*
** Bring up the previous window by tab order
*/
void Document::PreviousDocument() {

	if (!WindowList->next) {
		return;
	}

	Document *win = this->getNextTabWindow(-1, GetPrefGlobalTabNavigate(), 1);
	if(!win)
		return;

	if (this->shell == win->shell)
		win->RaiseDocument();
	else
		win->RaiseFocusDocumentWindow(True);
}

/*
** Bring up the next window by tab order
*/
void Document::NextDocument() {
	Document *win;

	if (!WindowList->next)
		return;

	win = this->getNextTabWindow(1, GetPrefGlobalTabNavigate(), 1);
	if(!win)
		return;

	if (this->shell == win->shell)
		win->RaiseDocument();
	else
		win->RaiseFocusDocumentWindow(True);
}

/*
** Change the window appearance and the window data structure to show
** that the file it contains has been modified
*/
void Document::SetWindowModified(int modified) {
	if (this->fileChanged == FALSE && modified == TRUE) {
		this->SetSensitive(this->closeItem, TRUE);
		this->fileChanged = TRUE;
		this->UpdateWindowTitle();
		this->RefreshTabState();
	} else if (this->fileChanged == TRUE && modified == FALSE) {
		this->fileChanged = FALSE;
		this->UpdateWindowTitle();
		this->RefreshTabState();
	}
}

/*
** Update the window title to reflect the filename, read-only, and modified
** status of the window data structure
*/
void Document::UpdateWindowTitle() {

	if (!this->IsTopDocument()) {
		return;
	}

	char *title = FormatWindowTitle(this->filename, this->path, GetClearCaseViewTag(), GetPrefServerName(), IsServer, this->filenameSet, this->lockReasons, this->fileChanged, GetPrefTitleFormat());
	char *iconTitle = new char[strlen(this->filename) + 2]; /* strlen("*")+1 */

	strcpy(iconTitle, this->filename);
	if (this->fileChanged) {
		strcat(iconTitle, "*");
	}
	
	XtVaSetValues(this->shell, XmNtitle, title, XmNiconName, iconTitle, nullptr);

	/* If there's a find or replace dialog up in "Keep Up" mode, with a
	   file name in the title, update it too */
	if (this->findDlog && XmToggleButtonGetState(this->findKeepBtn)) {
		sprintf(title, "Find (in %s)", this->filename);
		XtVaSetValues(XtParent(this->findDlog), XmNtitle, title, nullptr);
	}
	if (this->replaceDlog && XmToggleButtonGetState(this->replaceKeepBtn)) {
		sprintf(title, "Replace (in %s)", this->filename);
		XtVaSetValues(XtParent(this->replaceDlog), XmNtitle, title, nullptr);
	}
	delete [] iconTitle;

	/* Update the Windows menus with the new name */
	InvalidateWindowMenus();
}

/*
** Sort tabs in the tab bar alphabetically, if demanded so.
*/
void Document::SortTabBar() {

	if (!GetPrefSortTabs())
		return;

	/* need more than one tab to sort */
	const int nDoc = this->NDocuments();
	if (nDoc < 2) {
		return;
	}

	/* first sort the documents */
	std::vector<Document *> windows;
	windows.reserve(nDoc);
	
	for (Document *w = WindowList; w != nullptr; w = w->next) {
		if (this->shell == w->shell)
			windows.push_back(w);
	}
	
	std::sort(windows.begin(), windows.end(), [](const Document *a, const Document *b) {
		if(strcmp(a->filename, b->filename) < 0) {
			return true;
		}
		return strcmp(a->path, b->path) < 0;
	});
	
	/* assign tabs to documents in sorted order */
	WidgetList tabList;
	int tabCount;	
	XtVaGetValues(this->tabBar, XmNtabWidgetList, &tabList, XmNtabCount, &tabCount, nullptr);

	for (int i = 0, j = 0; i < tabCount && j < nDoc; i++) {
		if (tabList[i]->core.being_destroyed)
			continue;

		/* set tab as active */
		if (windows[j]->IsTopDocument())
			XmLFolderSetActiveTab(this->tabBar, i, False);

		windows[j]->tab = tabList[i];
		windows[j]->RefreshTabState();
		j++;
	}
}

/*
** Update the minimum allowable height for a split pane after a change
** to font or margin height.
*/
void Document::UpdateMinPaneHeights() {
	textDisp *textD = ((TextWidget)this->textArea)->text.textD;
	Dimension hsbHeight, swMarginHeight, frameShadowHeight;
	int i, marginHeight, minPaneHeight;
	Widget hScrollBar;

	/* find the minimum allowable size for a pane */
	XtVaGetValues(this->textArea, textNhScrollBar, &hScrollBar, nullptr);
	XtVaGetValues(containingPane(this->textArea), XmNscrolledWindowMarginHeight, &swMarginHeight, nullptr);
	XtVaGetValues(XtParent(this->textArea), XmNshadowThickness, &frameShadowHeight, nullptr);
	XtVaGetValues(this->textArea, textNmarginHeight, &marginHeight, nullptr);
	XtVaGetValues(hScrollBar, XmNheight, &hsbHeight, nullptr);
	minPaneHeight = textD->ascent + textD->descent + marginHeight * 2 + swMarginHeight * 2 + hsbHeight + 2 * frameShadowHeight;

	/* Set it in all of the widgets in the this */
	setPaneMinHeight(containingPane(this->textArea), minPaneHeight);
	for (i = 0; i < this->nPanes; i++)
		setPaneMinHeight(containingPane(this->textPanes[i]), minPaneHeight);
}

/*
** Update the "New (in X)" menu item to reflect the preferences
*/
void Document::UpdateNewOppositeMenu(int openInTab) {
	XmString lbl;
	if (openInTab)
		XtVaSetValues(this->newOppositeItem, XmNlabelString, lbl = XmStringCreateSimpleEx("New Window"), XmNmnemonic, 'W', nullptr);
	else
		XtVaSetValues(this->newOppositeItem, XmNlabelString, lbl = XmStringCreateSimpleEx("New Tab"), XmNmnemonic, 'T', nullptr);
	XmStringFree(lbl);
}

/*
** Set insert/overstrike mode
*/
void Document::SetOverstrike(int overstrike) {

	XtVaSetValues(this->textArea, textNoverstrike, overstrike, nullptr);
	for (int i = 0; i < this->nPanes; i++)
		XtVaSetValues(this->textPanes[i], textNoverstrike, overstrike, nullptr);
	this->overstrike = overstrike;
}

/*
** Select auto-wrap mode, one of NO_WRAP, NEWLINE_WRAP, or CONTINUOUS_WRAP
*/
void Document::SetAutoWrap(int state) {
	int autoWrap = state == NEWLINE_WRAP, contWrap = state == CONTINUOUS_WRAP;

	XtVaSetValues(this->textArea, textNautoWrap, autoWrap, textNcontinuousWrap, contWrap, nullptr);
	for (int i = 0; i < this->nPanes; i++)
		XtVaSetValues(this->textPanes[i], textNautoWrap, autoWrap, textNcontinuousWrap, contWrap, nullptr);
	this->wrapMode = state;

	if (this->IsTopDocument()) {
		XmToggleButtonSetState(this->newlineWrapItem, autoWrap, False);
		XmToggleButtonSetState(this->continuousWrapItem, contWrap, False);
		XmToggleButtonSetState(this->noWrapItem, state == NO_WRAP, False);
	}
}

/*
** Set the auto-scroll margin
*/
void Document::SetAutoScroll(int margin) {

	XtVaSetValues(this->textArea, textNcursorVPadding, margin, nullptr);
	for (int i = 0; i < this->nPanes; i++)
		XtVaSetValues(this->textPanes[i], textNcursorVPadding, margin, nullptr);
}

/*
**
*/
void Document::SetColors(const char *textFg, const char *textBg, const char *selectFg, const char *selectBg, const char *hiliteFg, const char *hiliteBg, const char *lineNoFg, const char *cursorFg) {
	
	int i, dummy;
	
	Pixel textFgPix = AllocColor(this->textArea, textFg, &dummy, &dummy, &dummy), textBgPix = AllocColor(this->textArea, textBg, &dummy, &dummy, &dummy), selectFgPix = AllocColor(this->textArea, selectFg, &dummy, &dummy, &dummy);
	Pixel selectBgPix = AllocColor(this->textArea, selectBg, &dummy, &dummy, &dummy), hiliteFgPix = AllocColor(this->textArea, hiliteFg, &dummy, &dummy, &dummy);
	Pixel hiliteBgPix = AllocColor(this->textArea, hiliteBg, &dummy, &dummy, &dummy), lineNoFgPix = AllocColor(this->textArea, lineNoFg, &dummy, &dummy, &dummy);
	Pixel cursorFgPix = AllocColor(this->textArea, cursorFg, &dummy, &dummy, &dummy);
	
	textDisp *textD;

	/* Update the main pane */
	XtVaSetValues(this->textArea, XmNforeground, textFgPix, XmNbackground, textBgPix, nullptr);
	textD = ((TextWidget)this->textArea)->text.textD;
	textD->	TextDSetColors(textFgPix, textBgPix, selectFgPix, selectBgPix, hiliteFgPix, hiliteBgPix, lineNoFgPix, cursorFgPix);
	/* Update any additional panes */
	for (i = 0; i < this->nPanes; i++) {
		XtVaSetValues(this->textPanes[i], XmNforeground, textFgPix, XmNbackground, textBgPix, nullptr);
		textD = ((TextWidget)this->textPanes[i])->text.textD;
		textD->TextDSetColors(textFgPix, textBgPix, selectFgPix, selectBgPix, hiliteFgPix, hiliteBgPix, lineNoFgPix, cursorFgPix);
	}

	/* Redo any syntax highlighting */
	if (this->highlightData)
		UpdateHighlightStyles(this);
}

/*
** close all the documents in a window
*/
int Document::CloseAllDocumentInWindow() {
	Document *win;

	if (this->NDocuments() == 1) {
		/* only one document in the window */
		return CloseFileAndWindow(this, PROMPT_SBC_DIALOG_RESPONSE);
	} else {
		Widget winShell = this->shell;
		Document *topDocument;

		/* close all _modified_ documents belong to this window */
		for (win = WindowList; win;) {
			if (win->shell == winShell && win->fileChanged) {
				Document *next = win->next;
				if (!CloseFileAndWindow(win, PROMPT_SBC_DIALOG_RESPONSE))
					return False;
				win = next;
			} else
				win = win->next;
		}

		/* see there's still documents left in the window */
		for (win = WindowList; win; win = win->next)
			if (win->shell == winShell)
				break;

		if (win) {
			topDocument = GetTopDocument(winShell);

			/* close all non-top documents belong to this window */
			for (win = WindowList; win;) {
				if (win->shell == winShell && win != topDocument) {
					Document *next = win->next;
					if (!CloseFileAndWindow(win, PROMPT_SBC_DIALOG_RESPONSE))
						return False;
					win = next;
				} else
					win = win->next;
			}

			/* close the last document and its window */
			if (!CloseFileAndWindow(topDocument, PROMPT_SBC_DIALOG_RESPONSE))
				return False;
		}
	}

	return True;
}

/*
** spin off the document to a new window
*/
Document *Document::DetachDocument() {
	Document *win = nullptr;

	if (this->NDocuments() < 2)
		return nullptr;

	/* raise another document in the same shell this if the this
	   being detached is the top document */
	if (this->IsTopDocument()) {
		win = this->getNextTabWindow(1, 0, 0);
		win->RaiseDocument();
	}

	/* Create a new this */
	auto cloneWin = new Document(this->filename, nullptr, false);

	/* CreateWindow() simply adds the new this's pointer to the
	   head of WindowList. We need to adjust the detached this's
	   pointer, so that macro functions such as focus_window("last")
	   will travel across the documents per the sequence they're
	   opened. The new doc will appear to replace it's former self
	   as the old doc is closed. */
	WindowList = cloneWin->next;
	cloneWin->next = this->next;
	this->next = cloneWin;

	/* these settings should follow the detached document.
	   must be done before cloning this, else the height
	   of split panes may not come out correctly */
	cloneWin->ShowISearchLine(this->showISearchLine);
	cloneWin->ShowStatsLine(this->showStats);

	/* clone the document & its pref settings */
	this->cloneDocument(cloneWin);

	/* remove the document from the old this */
	this->fileChanged = False;
	CloseFileAndWindow(this, NO_SBC_DIALOG_RESPONSE);

	/* refresh former host this */
	if (win) {
		win->RefreshWindowStates();
	}

	/* this should keep the new document this fresh */
	cloneWin->RefreshWindowStates();
	cloneWin->RefreshTabState();
	cloneWin->SortTabBar();

	return cloneWin;
}

/*
** present dialog for selecting a target window to move this document
** into. Do nothing if there is only one shell window opened.
*/
void Document::MoveDocumentDialog() {
	Document *win;
	int i, nList = 0, ac;
	char tmpStr[MAXPATHLEN + 50];
	Widget parent, dialog, listBox, moveAllOption;
	XmString popupTitle, s1;
	Arg csdargs[20];
	int *position_list;
	int position_count;

	/* get the list of available shell windows, not counting
	   the document to be moved */
	int nWindows = NWindows();
	auto list         = new XmString[nWindows];
	auto shellWinList = new Document *[nWindows];

	for (win = WindowList; win; win = win->next) {
		if (!win->IsTopDocument() || win->shell == this->shell)
			continue;

		snprintf(tmpStr, sizeof(tmpStr), "%s%s", win->filenameSet ? win->path : "", win->filename);

		list[nList] = XmStringCreateSimpleEx(tmpStr);
		shellWinList[nList] = win;
		nList++;
	}

	/* stop here if there's no other this to move to */
	if (!nList) {
		delete [] list;
		delete [] shellWinList;
		return;
	}

	/* create the dialog */
	parent = this->shell;
	popupTitle = XmStringCreateSimpleEx("Move Document");
	snprintf(tmpStr, sizeof(tmpStr), "Move %s into this of", this->filename);
	s1 = XmStringCreateSimpleEx(tmpStr);
	ac = 0;
	XtSetArg(csdargs[ac], XmNdialogStyle, XmDIALOG_FULL_APPLICATION_MODAL);
	ac++;
	XtSetArg(csdargs[ac], XmNdialogTitle, popupTitle);
	ac++;
	XtSetArg(csdargs[ac], XmNlistLabelString, s1);
	ac++;
	XtSetArg(csdargs[ac], XmNlistItems, list);
	ac++;
	XtSetArg(csdargs[ac], XmNlistItemCount, nList);
	ac++;
	XtSetArg(csdargs[ac], XmNvisibleItemCount, 12);
	ac++;
	XtSetArg(csdargs[ac], XmNautoUnmanage, False);
	ac++;
	dialog = CreateSelectionDialog(parent, (String) "moveDocument", csdargs, ac);
	XtUnmanageChild(XmSelectionBoxGetChild(dialog, XmDIALOG_TEXT));
	XtUnmanageChild(XmSelectionBoxGetChild(dialog, XmDIALOG_HELP_BUTTON));
	XtUnmanageChild(XmSelectionBoxGetChild(dialog, XmDIALOG_SELECTION_LABEL));
	XtAddCallback(dialog, XmNokCallback, moveDocumentCB, this);
	XtAddCallback(dialog, XmNapplyCallback, moveDocumentCB, this);
	XtAddCallback(dialog, XmNcancelCallback, moveDocumentCB, this);
	XmStringFree(s1);
	XmStringFree(popupTitle);

	/* free the this list */
	for (i = 0; i < nList; i++)
		XmStringFree(list[i]);
	delete [] list;

	/* create the option box for moving all documents */
	s1 = XmStringCreateLtoREx("Move all documents in this this");
	moveAllOption = XtVaCreateWidget("moveAll", xmToggleButtonWidgetClass, dialog, XmNlabelString, s1, XmNalignment, XmALIGNMENT_BEGINNING, nullptr);
	XmStringFree(s1);

	if (this->NDocuments() > 1)
		XtManageChild(moveAllOption);

	/* disable option if only one document in the this */
	XtUnmanageChild(XmSelectionBoxGetChild(dialog, XmDIALOG_APPLY_BUTTON));

	s1 = XmStringCreateLtoREx("Move");
	XtVaSetValues(dialog, XmNokLabelString, s1, nullptr);
	XmStringFree(s1);

	/* default to the first this on the list */
	listBox = XmSelectionBoxGetChild(dialog, XmDIALOG_LIST);
	XmListSelectPos(listBox, 1, True);

	/* show the dialog */
	DoneWithMoveDocumentDialog = 0;
	ManageDialogCenteredOnPointer(dialog);
	while (!DoneWithMoveDocumentDialog)
		XtAppProcessEvent(XtWidgetToApplicationContext(parent), XtIMAll);

	/* get the this to move document into */
	XmListGetSelectedPos(listBox, &position_list, &position_count);
	auto targetWin = shellWinList[position_list[0] - 1];
	XtFree((char *)position_list);

	/* now move document(s) */
	if (DoneWithMoveDocumentDialog == XmCR_OK) {
		/* move top document */
		if (XmToggleButtonGetState(moveAllOption)) {
			/* move all documents */
			for (win = WindowList; win;) {
				if (win != this && win->shell == this->shell) {
					Document *next = win->next;
					win->MoveDocument(targetWin);
					win = next;
				} else
					win = win->next;
			}

			/* invoking document is the last to move */
			this->MoveDocument(targetWin);
		} else {
			this->MoveDocument(targetWin);
		}
	}

	delete [] shellWinList;
	XtDestroyWidget(dialog);
}

/*
** If the selection (or cursor position if there's no selection) is not
** fully shown, scroll to bring it in to view.  Note that as written,
** this won't work well with multi-line selections.  Modest re-write
** of the horizontal scrolling part would be quite easy to make it work
** well with rectangular selections.
*/
void Document::MakeSelectionVisible(Widget textPane) {
	int left, right, rectStart, rectEnd, horizOffset;
	bool isRect;
	int scrollOffset, leftX, rightX, y, rows, margin;
	int topLineNum, lastLineNum, rightLineNum, leftLineNum, linesToScroll;
	textDisp *textD = ((TextWidget)textPane)->text.textD;
	int topChar = TextFirstVisiblePos(textPane);
	int lastChar = TextLastVisiblePos(textPane);
	int targetLineNum;
	Dimension width;

	/* find out where the selection is */
	if (!this->buffer->BufGetSelectionPos(&left, &right, &isRect, &rectStart, &rectEnd)) {
		left = right = TextGetCursorPos(textPane);
		isRect = False;
	}

	/* Check vertical positioning unless the selection is already shown or
	   already covers the display.  If the end of the selection is below
	   bottom, scroll it in to view until the end selection is scrollOffset
	   lines from the bottom of the display or the start of the selection
	   scrollOffset lines from the top.  Calculate a pleasing distance from the
	   top or bottom of the window, to scroll the selection to (if scrolling is
	   necessary), around 1/3 of the height of the window */
	if (!((left >= topChar && right <= lastChar) || (left <= topChar && right >= lastChar))) {
		XtVaGetValues(textPane, textNrows, &rows, nullptr);
		scrollOffset = rows / 3;
		TextGetScroll(textPane, &topLineNum, &horizOffset);
		if (right > lastChar) {
			/* End of sel. is below bottom of screen */
			leftLineNum = topLineNum + TextDCountLines(textD, topChar, left, False);
			targetLineNum = topLineNum + scrollOffset;
			if (leftLineNum >= targetLineNum) {
				/* Start of sel. is not between top & target */
				linesToScroll = TextDCountLines(textD, lastChar, right, False) + scrollOffset;
				if (leftLineNum - linesToScroll < targetLineNum)
					linesToScroll = leftLineNum - targetLineNum;
				/* Scroll start of selection to the target line */
				TextSetScroll(textPane, topLineNum + linesToScroll, horizOffset);
			}
		} else if (left < topChar) {
			/* Start of sel. is above top of screen */
			lastLineNum = topLineNum + rows;
			rightLineNum = lastLineNum - TextDCountLines(textD, right, lastChar, False);
			targetLineNum = lastLineNum - scrollOffset;
			if (rightLineNum <= targetLineNum) {
				/* End of sel. is not between bottom & target */
				linesToScroll = TextDCountLines(textD, left, topChar, False) + scrollOffset;
				if (rightLineNum + linesToScroll > targetLineNum)
					linesToScroll = targetLineNum - rightLineNum;
				/* Scroll end of selection to the target line */
				TextSetScroll(textPane, topLineNum - linesToScroll, horizOffset);
			}
		}
	}

	/* If either end of the selection off screen horizontally, try to bring it
	   in view, by making sure both end-points are visible.  Using only end
	   points of a multi-line selection is not a great idea, and disaster for
	   rectangular selections, so this part of the routine should be re-written
	   if it is to be used much with either.  Note also that this is a second
	   scrolling operation, causing the display to jump twice.  It's done after
	   vertical scrolling to take advantage of TextPosToXY which requires it's
	   reqested position to be vertically on screen) */
	if (TextPosToXY(textPane, left, &leftX, &y) && TextPosToXY(textPane, right, &rightX, &y) && leftX <= rightX) {
		TextGetScroll(textPane, &topLineNum, &horizOffset);
		XtVaGetValues(textPane, XmNwidth, &width, textNmarginWidth, &margin, nullptr);
		if (leftX < margin + textD->lineNumLeft + textD->lineNumWidth)
			horizOffset -= margin + textD->lineNumLeft + textD->lineNumWidth - leftX;
		else if (rightX > width - margin)
			horizOffset += rightX - (width - margin);
		TextSetScroll(textPane, topLineNum, horizOffset);
	}

	/* make sure that the statistics line is up to date */
	this->UpdateStatsLine();
}

/*
** Move document to an other window.
**
** the moving document will receive certain window settings from
** its new host, i.e. the window size, stats and isearch lines.
*/
Document *Document::MoveDocument(Document *toWindow) {
	Document *win = nullptr, *cloneWin;

	/* prepare to move document */
	if (this->NDocuments() < 2) {
		/* hide the this to make it look like we are moving */
		XtUnmapWidget(this->shell);
	} else if (this->IsTopDocument()) {
		/* raise another document to replace the document being moved */
		win = this->getNextTabWindow(1, 0, 0);
		win->RaiseDocument();
	}

	/* relocate the document to target this */
	cloneWin = toWindow->CreateDocument(this->filename);
	cloneWin->ShowTabBar(cloneWin->GetShowTabBar());
	this->cloneDocument(cloneWin);

	/* CreateDocument() simply adds the new this's pointer to the
	   head of WindowList. We need to adjust the detached this's
	   pointer, so that macro functions such as focus_window("last")
	   will travel across the documents per the sequence they're
	   opened. The new doc will appear to replace it's former self
	   as the old doc is closed. */
	WindowList = cloneWin->next;
	cloneWin->next = this->next;
	this->next = cloneWin;

	/* remove the document from the old this */
	this->fileChanged = False;
	CloseFileAndWindow(this, NO_SBC_DIALOG_RESPONSE);

	/* some menu states might have changed when deleting document */
	if (win) {
		win->RefreshWindowStates();
	}

	/* this should keep the new document this fresh */
	cloneWin->RaiseDocumentWindow();
	cloneWin->RefreshTabState();
	cloneWin->SortTabBar();

	return cloneWin;
}

void Document::ShowWindowTabBar() {
	if (GetPrefTabBar()) {
		if (GetPrefTabBarHideOne()) {
			this->ShowTabBar(this->NDocuments() > 1);
		} else {
			this->ShowTabBar(True);
		}
	} else {
		this->ShowTabBar(False);
	}
}

/*
** Turn on and off the display of line numbers
*/
void Document::ShowLineNumbers(int state) {
	Widget text;
	int i, marginWidth;
	unsigned reqCols = 0;
	Dimension windowWidth;
	Document *win;
	textDisp *textD = ((TextWidget)this->textArea)->text.textD;

	if (this->showLineNumbers == state)
		return;
	this->showLineNumbers = state;

	/* Just setting this->showLineNumbers is sufficient to tell
	   updateLineNumDisp() to expand the line number areas and the this
	   size for the number of lines required.  To hide the line number
	   display, set the width to zero, and contract the this width. */
	if (state) {
		reqCols = this->updateLineNumDisp();
	} else {
		XtVaGetValues(this->shell, XmNwidth, &windowWidth, nullptr);
		XtVaGetValues(this->textArea, textNmarginWidth, &marginWidth, nullptr);
		XtVaSetValues(this->shell, XmNwidth, windowWidth - textD->left + marginWidth, nullptr);

		for (i = 0; i <= this->nPanes; i++) {
			text = i == 0 ? this->textArea : this->textPanes[i - 1];
			XtVaSetValues(text, textNlineNumCols, 0, nullptr);
		}
	}

	/* line numbers panel is shell-level, hence other
	   tabbed documents in the this should synch */
	for (win = WindowList; win; win = win->next) {
		if (win->shell != this->shell || win == this)
			continue;

		win->showLineNumbers = state;

		for (i = 0; i <= win->nPanes; i++) {
			text = i == 0 ? win->textArea : win->textPanes[i - 1];
			/*  reqCols should really be cast here, but into what? XmRInt?  */
			XtVaSetValues(text, textNlineNumCols, reqCols, nullptr);
		}
	}

	/* Tell WM that the non-expandable part of the this has changed size */
	this->UpdateWMSizeHints();
}

/*
** Turn on and off the display of the statistics line
*/
void Document::ShowStatsLine(int state) {
	Document *win;
	Widget text;
	int i;

	/* In continuous wrap mode, text widgets must be told to keep track of
	   the top line number in absolute (non-wrapped) lines, because it can
	   be a costly calculation, and is only needed for displaying line
	   numbers, either in the stats line, or along the left margin */
	for (i = 0; i <= this->nPanes; i++) {
		text = (i == 0) ? this->textArea : this->textPanes[i - 1];
		reinterpret_cast<TextWidget>(text)->text.textD->TextDMaintainAbsLineNum(state);
	}
	this->showStats = state;
	this->showStatistics(state);

	/* i-search line is shell-level, hence other tabbed
	   documents in the this should synch */
	for (win = WindowList; win; win = win->next) {
		if (win->shell != this->shell || win == this)
			continue;
		win->showStats = state;
	}
}

/*
** Set autoindent state to one of  NO_AUTO_INDENT, AUTO_INDENT, or SMART_INDENT.
*/
void Document::SetAutoIndent(int state) {
	int autoIndent = state == AUTO_INDENT, smartIndent = state == SMART_INDENT;
	int i;

	if (this->indentStyle == SMART_INDENT && !smartIndent)
		EndSmartIndent(this);
	else if (smartIndent && this->indentStyle != SMART_INDENT)
		BeginSmartIndent(this, True);
	this->indentStyle = state;
	XtVaSetValues(this->textArea, textNautoIndent, autoIndent, textNsmartIndent, smartIndent, nullptr);
	for (i = 0; i < this->nPanes; i++)
		XtVaSetValues(this->textPanes[i], textNautoIndent, autoIndent, textNsmartIndent, smartIndent, nullptr);
	if (this->IsTopDocument()) {
		XmToggleButtonSetState(this->smartIndentItem, smartIndent, False);
		XmToggleButtonSetState(this->autoIndentItem, autoIndent, False);
		XmToggleButtonSetState(this->autoIndentOffItem, state == NO_AUTO_INDENT, False);
	}
}

/*
** Set showMatching state to one of NO_FLASH, FLASH_DELIMIT or FLASH_RANGE.
** Update the menu to reflect the change of state.
*/
void Document::SetShowMatching(int state) {
	this->showMatchingStyle = state;
	if (this->IsTopDocument()) {
		XmToggleButtonSetState(this->showMatchingOffItem, state == NO_FLASH, False);
		XmToggleButtonSetState(this->showMatchingDelimitItem, state == FLASH_DELIMIT, False);
		XmToggleButtonSetState(this->showMatchingRangeItem, state == FLASH_RANGE, False);
	}
}

/*
** Set the fonts for "window" from a font name, and updates the display.
** Also updates window->fontList which is used for statistics line.
**
** Note that this leaks memory and server resources.  In previous NEdit
** versions, fontLists were carefully tracked and freed, but X and Motif
** have some kind of timing problem when widgets are distroyed, such that
** fonts may not be freed immediately after widget destruction with 100%
** safety.  Rather than kludge around this with timerProcs, I have chosen
** to create new fontLists only when the user explicitly changes the font
** (which shouldn't happen much in normal NEdit operation), and skip the
** futile effort of freeing them.
*/
void Document::SetFonts(const char *fontName, const char *italicName, const char *boldName, const char *boldItalicName) {

	XFontStruct *font, *oldFont;
	int i, oldFontWidth, oldFontHeight, fontWidth, fontHeight;
	int borderWidth, borderHeight, marginWidth, marginHeight;
	int primaryChanged, highlightChanged = False;
	Dimension oldWindowWidth, oldWindowHeight, oldTextWidth, oldTextHeight;
	Dimension textHeight, newWindowWidth, newWindowHeight;
	textDisp *textD = ((TextWidget)this->textArea)->text.textD;

	/* Check which fonts have changed */
	primaryChanged = strcmp(fontName, this->fontName);
	if (strcmp(italicName, this->italicFontName))
		highlightChanged = True;
	if (strcmp(boldName, this->boldFontName))
		highlightChanged = True;
	if (strcmp(boldItalicName, this->boldItalicFontName))
		highlightChanged = True;
	if (!primaryChanged && !highlightChanged)
		return;

	/* Get information about the current this sizing, to be used to
	   determine the correct this size after the font is changed */
	XtVaGetValues(this->shell, XmNwidth, &oldWindowWidth, XmNheight, &oldWindowHeight, nullptr);
	XtVaGetValues(this->textArea, XmNheight, &textHeight, textNmarginHeight, &marginHeight, textNmarginWidth, &marginWidth, textNfont, &oldFont, nullptr);
	oldTextWidth = textD->width + textD->lineNumWidth;
	oldTextHeight = textHeight - 2 * marginHeight;
	for (i = 0; i < this->nPanes; i++) {
		XtVaGetValues(this->textPanes[i], XmNheight, &textHeight, nullptr);
		oldTextHeight += textHeight - 2 * marginHeight;
	}
	borderWidth = oldWindowWidth - oldTextWidth;
	borderHeight = oldWindowHeight - oldTextHeight;
	oldFontWidth = oldFont->max_bounds.width;
	oldFontHeight = textD->ascent + textD->descent;

	/* Change the fonts in the this data structure.  If the primary font
	   didn't work, use Motif's fallback mechanism by stealing it from the
	   statistics line.  Highlight fonts are allowed to be nullptr, which
	   is interpreted as "use the primary font" */
	if (primaryChanged) {
		strcpy(this->fontName, fontName);
		font = XLoadQueryFont(TheDisplay, fontName);
		if(!font)
			XtVaGetValues(this->statsLine, XmNfontList, &this->fontList, nullptr);
		else
			this->fontList = XmFontListCreate(font, XmSTRING_DEFAULT_CHARSET);
	}
	if (highlightChanged) {
		strcpy(this->italicFontName, italicName);
		this->italicFontStruct = XLoadQueryFont(TheDisplay, italicName);
		strcpy(this->boldFontName, boldName);
		this->boldFontStruct = XLoadQueryFont(TheDisplay, boldName);
		strcpy(this->boldItalicFontName, boldItalicName);
		this->boldItalicFontStruct = XLoadQueryFont(TheDisplay, boldItalicName);
	}

	/* Change the primary font in all the widgets */
	if (primaryChanged) {
		font = GetDefaultFontStruct(this->fontList);
		XtVaSetValues(this->textArea, textNfont, font, nullptr);
		for (i = 0; i < this->nPanes; i++)
			XtVaSetValues(this->textPanes[i], textNfont, font, nullptr);
	}

	/* Change the highlight fonts, even if they didn't change, because
	   primary font is read through the style table for syntax highlighting */
	if (this->highlightData)
		UpdateHighlightStyles(this);

	/* Change the this manager size hints.
	   Note: this has to be done _before_ we set the new sizes. ICCCM2
	   compliant this managers (such as fvwm2) would otherwise resize
	   the this twice: once because of the new sizes requested, and once
	   because of the new size increments, resulting in an overshoot. */
	this->UpdateWMSizeHints();

	/* Use the information from the old this to re-size the this to a
	   size appropriate for the new font, but only do so if there's only
	   _one_ document in the this, in order to avoid growing-this bug */
	if (this->NDocuments() == 1) {
		fontWidth = GetDefaultFontStruct(this->fontList)->max_bounds.width;
		fontHeight = textD->ascent + textD->descent;
		newWindowWidth = (oldTextWidth * fontWidth) / oldFontWidth + borderWidth;
		newWindowHeight = (oldTextHeight * fontHeight) / oldFontHeight + borderHeight;
		XtVaSetValues(this->shell, XmNwidth, newWindowWidth, XmNheight, newWindowHeight, nullptr);
	}

	/* Change the minimum pane height */
	this->UpdateMinPaneHeights();
}

/*
** Close the window pane that last had the keyboard focus.  (Actually, close
** the bottom pane and make it look like pane which had focus was closed)
*/
void Document::ClosePane() {
	short paneHeights[MAX_PANES + 1];
	int insertPositions[MAX_PANES + 1], topLines[MAX_PANES + 1];
	int horizOffsets[MAX_PANES + 1];
	int i, focusPane;
	Widget text;

	/* Don't delete the last pane */
	if (this->nPanes <= 0)
		return;

	/* Record the current heights, scroll positions, and insert positions
	   of the existing panes, and the keyboard focus */
	focusPane = 0;
	for (i = 0; i <= this->nPanes; i++) {
		text = i == 0 ? this->textArea : this->textPanes[i - 1];
		insertPositions[i] = TextGetCursorPos(text);
		XtVaGetValues(containingPane(text), XmNheight, &paneHeights[i], nullptr);
		TextGetScroll(text, &topLines[i], &horizOffsets[i]);
		if (text == this->lastFocus)
			focusPane = i;
	}

	/* Unmanage & remanage the panedWindow so it recalculates pane heights */
	XtUnmanageChild(this->splitPane);

	/* Destroy last pane, and make sure lastFocus points to an existing pane.
	   Workaround for OM 2.1.30: text widget must be unmanaged for
	   xmPanedWindowWidget to calculate the correct pane heights for
	   the remaining panes, simply detroying it didn't seem enough */
	this->nPanes--;
	XtUnmanageChild(containingPane(this->textPanes[this->nPanes]));
	XtDestroyWidget(containingPane(this->textPanes[this->nPanes]));

	if (this->nPanes == 0)
		this->lastFocus = this->textArea;
	else if (focusPane > this->nPanes)
		this->lastFocus = this->textPanes[this->nPanes - 1];

	/* adjust the heights, scroll positions, etc., to make it look
	   like the pane with the input focus was closed */
	for (i = focusPane; i <= this->nPanes; i++) {
		insertPositions[i] = insertPositions[i + 1];
		paneHeights[i] = paneHeights[i + 1];
		topLines[i] = topLines[i + 1];
		horizOffsets[i] = horizOffsets[i + 1];
	}

	/* set the desired heights and re-manage the paned window so it will
	   recalculate pane heights */
	for (i = 0; i <= this->nPanes; i++) {
		text = i == 0 ? this->textArea : this->textPanes[i - 1];
		setPaneDesiredHeight(containingPane(text), paneHeights[i]);
	}

	if (this->IsTopDocument())
		XtManageChild(this->splitPane);

	/* Reset all of the scroll positions, insert positions, etc. */
	for (i = 0; i <= this->nPanes; i++) {
		text = i == 0 ? this->textArea : this->textPanes[i - 1];
		TextSetCursorPos(text, insertPositions[i]);
		TextSetScroll(text, topLines[i], horizOffsets[i]);
	}
	XmProcessTraversal(this->lastFocus, XmTRAVERSE_CURRENT);

	/* Update the window manager size hints after the sizes of the panes have
	   been set (the widget heights are not yet readable here, but they will
	   be by the time the event loop gets around to running this timer proc) */
	XtAppAddTimeOut(XtWidgetToApplicationContext(this->shell), 0, wmSizeUpdateProc, this);
}

/*
** Close a document, or an editor window
*/
void Document::CloseWindow() {
	int keepWindow, state;
	char name[MAXPATHLEN];
	Document *win;
	Document *topBuf = nullptr;
	Document *nextBuf = nullptr;

	/* Free smart indent macro programs */
	EndSmartIndent(this);

	/* Clean up macro references to the doomed window.  If a macro is
	   executing, stop it.  If macro is calling this (closing its own
	   window), leave the window alive until the macro completes */
	keepWindow = !MacroWindowCloseActions(this);

	/* Kill shell sub-process and free related memory */
	AbortShellCommand(this);

	/* Unload the default tips files for this language mode if necessary */
	UnloadLanguageModeTipsFile(this);

	/* If a window is closed while it is on the multi-file replace dialog
	   list of any other window (or even the same one), we must update those
	   lists or we end up with dangling references. Normally, there can
	   be only one of those dialogs at the same time (application modal),
	   but LessTif doesn't even (always) honor application modalness, so
	   there can be more than one dialog. */
	RemoveFromMultiReplaceDialog(this);

	/* Destroy the file closed property for this file */
	DeleteFileClosedProperty(this);

	/* Remove any possibly pending callback which might fire after the
	   widget is gone. */
	cancelTimeOut(&this->flashTimeoutID);
	cancelTimeOut(&this->markTimeoutID);

	/* if this is the last window, or must be kept alive temporarily because
	   it's running the macro calling us, don't close it, make it Untitled */
	if (keepWindow || (WindowList == this && this->next == nullptr)) {
		this->filename[0] = '\0';
		UniqueUntitledName(name, sizeof(name));
		CLEAR_ALL_LOCKS(this->lockReasons);
		this->fileMode = 0;
		this->fileUid = 0;
		this->fileGid = 0;
		strcpy(this->filename, name);
		strcpy(this->path, "");
		this->ignoreModify = TRUE;
		this->buffer->BufSetAllEx("");
		this->ignoreModify = FALSE;
		this->nMarks = 0;
		this->filenameSet = FALSE;
		this->fileMissing = TRUE;
		this->fileChanged = FALSE;
		this->fileFormat = UNIX_FILE_FORMAT;
		this->lastModTime = 0;
		this->device = 0;
		this->inode = 0;

		StopHighlighting(this);
		EndSmartIndent(this);
		this->UpdateWindowTitle();
		this->UpdateWindowReadOnly();
		XtSetSensitive(this->closeItem, FALSE);
		XtSetSensitive(this->readOnlyItem, TRUE);
		XmToggleButtonSetState(this->readOnlyItem, FALSE, FALSE);
		this->ClearUndoList();
		this->ClearRedoList();
		XmTextSetStringEx(this->statsLine, ""); /* resets scroll pos of stats
		                                            line from long file names */
		this->UpdateStatsLine();
		DetermineLanguageMode(this, True);
		this->RefreshTabState();
		this->updateLineNumDisp();
		return;
	}

	/* Free syntax highlighting patterns, if any. w/o redisplaying */
	FreeHighlightingData(this);

	/* remove the buffer modification callbacks so the buffer will be
	   deallocated when the last text widget is destroyed */
	this->buffer->BufRemoveModifyCB(modifiedCB, this);
	this->buffer->BufRemoveModifyCB(SyntaxHighlightModifyCB, this);


	/* free the undo and redo lists */
	this->ClearUndoList();
	this->ClearRedoList();

	/* close the document/window */
	if (this->NDocuments() > 1) {
		if (MacroRunWindow() && MacroRunWindow() != this && MacroRunWindow()->shell == this->shell) {
			nextBuf = MacroRunWindow();
			nextBuf->RaiseDocument();
		} else if (this->IsTopDocument()) {
			/* need to find a successor before closing a top document */
			nextBuf = this->getNextTabWindow(1, 0, 0);
			nextBuf->RaiseDocument();
		} else {
			topBuf = GetTopDocument(this->shell);
		}
	}

	/* remove the window from the global window list, update window menus */
	this->removeFromWindowList();
	InvalidateWindowMenus();
	CheckCloseDim(); /* Close of window running a macro may have been disabled. */

	/* remove the tab of the closing document from tab bar */
	XtDestroyWidget(this->tab);

	/* refresh tab bar after closing a document */
	if (nextBuf) {
		nextBuf->ShowWindowTabBar();
		nextBuf->updateLineNumDisp();
	} else if (topBuf) {
		topBuf->ShowWindowTabBar();
		topBuf->updateLineNumDisp();
	}

	/* dim/undim Detach_Tab menu items */
	win = nextBuf ? nextBuf : topBuf;
	if (win) {
		state = win->NDocuments() > 1;
		XtSetSensitive(win->detachDocumentItem, state);
		XtSetSensitive(win->contextDetachDocumentItem, state);
	}

	/* dim/undim Attach_Tab menu items */
	state = WindowList->NDocuments() < NWindows();
	for (win = WindowList; win; win = win->next) {
		if (win->IsTopDocument()) {
			XtSetSensitive(win->moveDocumentItem, state);
			XtSetSensitive(win->contextMoveDocumentItem, state);
		}
	}

	/* free background menu cache for document */
	FreeUserBGMenuCache(&this->userBGMenuCache);

	/* destroy the document's pane, or the window */
	if (nextBuf || topBuf) {
		this->deleteDocument();
	} else {
		/* free user menu cache for window */
		FreeUserMenuCache(this->userMenuCache);

		/* remove and deallocate all of the widgets associated with window */
		XtFree(this->backlightCharTypes); /* we made a copy earlier on */
		CloseAllPopupsFor(this->shell);
		XtDestroyWidget(this->shell);
	}

	/* deallocate the window data structure */
	delete this;
}

/*
**
*/
void Document::deleteDocument() {
	if (!this) {
		return;
	}

	XtDestroyWidget(this->splitPane);
}

/*
** Calculate the dimension of the text area, in terms of rows & cols,
** as if there's only one single text pane in the window.
*/
void Document::getTextPaneDimension(int *nRows, int *nCols) {
	Widget hScrollBar;
	Dimension hScrollBarHeight, paneHeight;
	int marginHeight, marginWidth, totalHeight, fontHeight;
	textDisp *textD = ((TextWidget)this->textArea)->text.textD;

	/* width is the same for panes */
	XtVaGetValues(this->textArea, textNcolumns, nCols, nullptr);

	/* we have to work out the height, as the text area may have been split */
	XtVaGetValues(this->textArea, textNhScrollBar, &hScrollBar, textNmarginHeight, &marginHeight, textNmarginWidth, &marginWidth, nullptr);
	XtVaGetValues(hScrollBar, XmNheight, &hScrollBarHeight, nullptr);
	XtVaGetValues(this->splitPane, XmNheight, &paneHeight, nullptr);
	totalHeight = paneHeight - 2 * marginHeight - hScrollBarHeight;
	fontHeight = textD->ascent + textD->descent;
	*nRows = totalHeight / fontHeight;
}

/*
** Add another independently scrollable pane to the current document,
** splitting the pane which currently has keyboard focus.
*/
void Document::SplitPane() {
	short paneHeights[MAX_PANES + 1];
	int insertPositions[MAX_PANES + 1], topLines[MAX_PANES + 1];
	int horizOffsets[MAX_PANES + 1];
	int i, focusPane, emTabDist, wrapMargin, lineNumCols, totalHeight = 0;
	char *delimiters;
	Widget text = nullptr;
	textDisp *textD, *newTextD;

	/* Don't create new panes if we're already at the limit */
	if (this->nPanes >= MAX_PANES)
		return;

	/* Record the current heights, scroll positions, and insert positions
	   of the existing panes, keyboard focus */
	focusPane = 0;
	for (i = 0; i <= this->nPanes; i++) {
		text = i == 0 ? this->textArea : this->textPanes[i - 1];
		insertPositions[i] = TextGetCursorPos(text);
		XtVaGetValues(containingPane(text), XmNheight, &paneHeights[i], nullptr);
		totalHeight += paneHeights[i];
		TextGetScroll(text, &topLines[i], &horizOffsets[i]);
		if (text == this->lastFocus)
			focusPane = i;
	}

	/* Unmanage & remanage the panedWindow so it recalculates pane heights */
	XtUnmanageChild(this->splitPane);

	/* Create a text widget to add to the pane and set its buffer and
	   highlight data to be the same as the other panes in the document */
	XtVaGetValues(this->textArea, textNemulateTabs, &emTabDist, textNwordDelimiters, &delimiters, textNwrapMargin, &wrapMargin, textNlineNumCols, &lineNumCols, nullptr);
	text = createTextArea(this->splitPane, this, 1, 1, emTabDist, delimiters, wrapMargin, lineNumCols);

	TextSetBuffer(text, this->buffer);
	if (this->highlightData)
		AttachHighlightToWidget(text, this);
	if (this->backlightChars) {
		XtVaSetValues(text, textNbacklightCharTypes, this->backlightCharTypes, nullptr);
	}
	XtManageChild(text);
	this->textPanes[this->nPanes++] = text;

	/* Fix up the colors */
	textD = ((TextWidget)this->textArea)->text.textD;
	newTextD = reinterpret_cast<TextWidget>(text)->text.textD;
	XtVaSetValues(text, XmNforeground, textD->fgPixel, XmNbackground, textD->bgPixel, nullptr);
	newTextD->TextDSetColors(textD->fgPixel, textD->bgPixel, textD->selectFGPixel, textD->selectBGPixel, textD->highlightFGPixel, textD->highlightBGPixel, textD->lineNumFGPixel, textD->cursorFGPixel);

	/* Set the minimum pane height in the new pane */
	this->UpdateMinPaneHeights();

	/* adjust the heights, scroll positions, etc., to split the focus pane */
	for (i = this->nPanes; i > focusPane; i--) {
		insertPositions[i] = insertPositions[i - 1];
		paneHeights[i] = paneHeights[i - 1];
		topLines[i] = topLines[i - 1];
		horizOffsets[i] = horizOffsets[i - 1];
	}
	paneHeights[focusPane] = paneHeights[focusPane] / 2;
	paneHeights[focusPane + 1] = paneHeights[focusPane];

	for (i = 0; i <= this->nPanes; i++) {
		text = i == 0 ? this->textArea : this->textPanes[i - 1];
		setPaneDesiredHeight(containingPane(text), paneHeights[i]);
	}

	/* Re-manage panedWindow to recalculate pane heights & reset selection */
	if (this->IsTopDocument())
		XtManageChild(this->splitPane);

	/* Reset all of the heights, scroll positions, etc. */
	for (i = 0; i <= this->nPanes; i++) {
		text = i == 0 ? this->textArea : this->textPanes[i - 1];
		TextSetCursorPos(text, insertPositions[i]);
		TextSetScroll(text, topLines[i], horizOffsets[i]);
		setPaneDesiredHeight(containingPane(text), totalHeight / (this->nPanes + 1));
	}
	XmProcessTraversal(this->lastFocus, XmTRAVERSE_CURRENT);

	/* Update the this manager size hints after the sizes of the panes have
	   been set (the widget heights are not yet readable here, but they will
	   be by the time the event loop gets around to running this timer proc) */
	XtAppAddTimeOut(XtWidgetToApplicationContext(this->shell), 0, wmSizeUpdateProc, this);
}

/*
** Save the position and size of a window as an X standard geometry string.
** A string of at least MAX_GEOMETRY_STRING_LEN characters should be
** provided in the argument "geomString" to receive the result.
*/
void Document::getGeometryString(char *geomString) {
	int x, y, fontWidth, fontHeight, baseWidth, baseHeight;
	unsigned int width, height, dummyW, dummyH, bw, depth, nChild;
	Window parent, root, *child, w = XtWindow(this->shell);
	Display *dpy = XtDisplay(this->shell);

	/* Find the width and height from the this of the shell */
	XGetGeometry(dpy, w, &root, &x, &y, &width, &height, &bw, &depth);

	/* Find the top left corner (x and y) of the this decorations.  (This
	   is what's required in the geometry string to restore the this to it's
	   original position, since the this manager re-parents the this to
	   add it's title bar and menus, and moves the requested this down and
	   to the left.)  The position is found by traversing the this hier-
	   archy back to the this to the last parent before the root this */
	for (;;) {
		XQueryTree(dpy, w, &root, &parent, &child, &nChild);
		XFree((char *)child);
		if (parent == root)
			break;
		w = parent;
	}
	XGetGeometry(dpy, w, &root, &x, &y, &dummyW, &dummyH, &bw, &depth);

	/* Use this manager size hints (set by UpdateWMSizeHints) to
	   translate the width and height into characters, as opposed to pixels */
	XtVaGetValues(this->shell, XmNwidthInc, &fontWidth, XmNheightInc, &fontHeight, XmNbaseWidth, &baseWidth, XmNbaseHeight, &baseHeight, nullptr);
	width = (width - baseWidth) / fontWidth;
	height = (height - baseHeight) / fontHeight;

	/* Write the string */
	CreateGeometryString(geomString, x, y, width, height, XValue | YValue | WidthValue | HeightValue);
}

/*
** Raise a tabbed document within its shell window.
**
** NB: use RaiseDocumentWindow() to raise the doc and
**     its shell window.
*/
void Document::RaiseDocument() {
	Document *win, *lastwin;

	if (!this || !WindowList)
		return;

	lastwin = this->MarkActiveDocument();
	if (lastwin != this && lastwin->IsValidWindow())
		lastwin->MarkLastDocument();

	/* document already on top? */
	XtVaGetValues(this->mainWin, XmNuserData, &win, nullptr);
	if (win == this)
		return;

	/* set the document as top document */
	XtVaSetValues(this->mainWin, XmNuserData, this, nullptr);

	/* show the new top document */
	XtVaSetValues(this->mainWin, XmNworkWindow, this->splitPane, nullptr);
	XtManageChild(this->splitPane);
	XRaiseWindow(TheDisplay, XtWindow(this->splitPane));

	/* Turn on syntax highlight that might have been deferred.
	   NB: this must be done after setting the document as
	       XmNworkWindow and managed, else the parent shell
	   this may shrink on some this-managers such as
	   metacity, due to changes made in UpdateWMSizeHints().*/
	if (this->highlightSyntax && this->highlightData == nullptr)
		StartHighlighting(this, False);

	/* put away the bg menu tearoffs of last active document */
	hideTearOffs(win->bgMenuPane);

	/* restore the bg menu tearoffs of active document */
	redisplayTearOffs(this->bgMenuPane);

	/* set tab as active */
	XmLFolderSetActiveTab(this->tabBar, getTabPosition(this->tab), False);

	/* set keyboard focus. Must be done before unmanaging previous
	   top document, else lastFocus will be reset to textArea */
	XmProcessTraversal(this->lastFocus, XmTRAVERSE_CURRENT);

	/* we only manage the top document, else the next time a document
	   is raised again, it's textpane might not resize properly.
	   Also, somehow (bug?) XtUnmanageChild() doesn't hide the
	   splitPane, which obscure lower part of the statsform when
	   we toggle its components, so we need to put the document at
	   the back */
	XLowerWindow(TheDisplay, XtWindow(win->splitPane));
	XtUnmanageChild(win->splitPane);
	win->RefreshTabState();

	/* now refresh this state/info. RefreshWindowStates()
	   has a lot of work to do, so we update the screen first so
	   the document appears to switch swiftly. */
	XmUpdateDisplay(this->splitPane);
	this->RefreshWindowStates();
	this->RefreshTabState();

	/* put away the bg menu tearoffs of last active document */
	hideTearOffs(win->bgMenuPane);

	/* restore the bg menu tearoffs of active document */
	redisplayTearOffs(this->bgMenuPane);

	/* Make sure that the "In Selection" button tracks the presence of a
	   selection and that the this inherits the proper search scope. */
	if (this->replaceDlog != nullptr && XtIsManaged(this->replaceDlog)) {
#ifdef REPLACE_SCOPE
		this->replaceScope = win->replaceScope;
#endif
		UpdateReplaceActionButtons(this);
	}

	this->UpdateWMSizeHints();
}

/*
** clone a document's states and settings into the other.
*/
void Document::cloneDocument(Document *window) {
	char *params[4];
	int emTabDist;

	strcpy(window->path,     this->path);
	strcpy(window->filename, this->filename);

	window->ShowLineNumbers(this->showLineNumbers);

	window->ignoreModify = True;

	/* copy the text buffer */
	auto orgDocument = this->buffer->BufAsStringEx();
	window->buffer->BufSetAllEx(orgDocument);

	/* copy the tab preferences (here!) */
	window->buffer->BufSetTabDistance(this->buffer->tabDist_);
	window->buffer->useTabs_ = this->buffer->useTabs_;
	XtVaGetValues(this->textArea, textNemulateTabs, &emTabDist, nullptr);
	window->SetEmTabDist(emTabDist);

	window->ignoreModify = False;

	/* transfer text fonts */
	params[0] = this->fontName;
	params[1] = this->italicFontName;
	params[2] = this->boldFontName;
	params[3] = this->boldItalicFontName;
	XtCallActionProc(window->textArea, "set_fonts", nullptr, params, 4);

	window->SetBacklightChars(this->backlightCharTypes);

	/* Clone rangeset info.

	   FIXME:
	   Cloning of rangesets must be done before syntax highlighting,
	   else the rangesets do not be highlighted (colored) properly
	   if syntax highlighting is on.
	*/
	if(this->buffer->rangesetTable_) {
		window->buffer->rangesetTable_ = new RangesetTable(window->buffer, *this->buffer->rangesetTable_);
	} else {
		window->buffer->rangesetTable_ = nullptr;
	}

	/* Syntax highlighting */
	window->languageMode = this->languageMode;
	window->highlightSyntax = this->highlightSyntax;
	if (window->highlightSyntax) {
		StartHighlighting(window, False);
	}

	/* copy states of original document */
	window->filenameSet = this->filenameSet;
	window->fileFormat = this->fileFormat;
	window->lastModTime = this->lastModTime;
	window->fileChanged = this->fileChanged;
	window->fileMissing = this->fileMissing;
	window->lockReasons = this->lockReasons;
	window->autoSaveCharCount = this->autoSaveCharCount;
	window->autoSaveOpCount = this->autoSaveOpCount;
	window->undoMemUsed = this->undoMemUsed;
	window->lockReasons = this->lockReasons;
	window->autoSave = this->autoSave;
	window->saveOldVersion = this->saveOldVersion;
	window->wrapMode = this->wrapMode;
	window->SetOverstrike(this->overstrike);
	window->showMatchingStyle = this->showMatchingStyle;
	window->matchSyntaxBased = this->matchSyntaxBased;
#if 0    
    window->showStats = this->showStats;
    window->showISearchLine = this->showISearchLine;
    window->showLineNumbers = this->showLineNumbers;
    window->modeMessageDisplayed = this->modeMessageDisplayed;
    window->ignoreModify = this->ignoreModify;
    window->windowMenuValid = this->windowMenuValid;
    window->flashTimeoutID = this->flashTimeoutID;
    window->wasSelected = this->wasSelected;
    strcpy(window->fontName, this->fontName);
    strcpy(window->italicFontName, this->italicFontName);
    strcpy(window->boldFontName, this->boldFontName);
    strcpy(window->boldItalicFontName, this->boldItalicFontName);
    window->fontList = this->fontList;
    window->italicFontStruct = this->italicFontStruct;
    window->boldFontStruct = this->boldFontStruct;
    window->boldItalicFontStruct = this->boldItalicFontStruct;
    window->markTimeoutID = this->markTimeoutID;
    window->highlightData = this->highlightData;
    window->shellCmdData = this->shellCmdData;
    window->macroCmdData = this->macroCmdData;
    window->smartIndentData = this->smartIndentData;
#endif
	window->iSearchHistIndex = this->iSearchHistIndex;
	window->iSearchStartPos = this->iSearchStartPos;
	window->replaceLastRegexCase = this->replaceLastRegexCase;
	window->replaceLastLiteralCase = this->replaceLastLiteralCase;
	window->iSearchLastRegexCase = this->iSearchLastRegexCase;
	window->iSearchLastLiteralCase = this->iSearchLastLiteralCase;
	window->findLastRegexCase = this->findLastRegexCase;
	window->findLastLiteralCase = this->findLastLiteralCase;
	window->device = this->device;
	window->inode = this->inode;
	window->fileClosedAtom = this->fileClosedAtom;
	this->fileClosedAtom = None;

	/* copy the text/split panes settings, cursor pos & selection */
	cloneTextPanes(window, this);

	/* copy undo & redo list */
	window->undo = cloneUndoItems(this->undo);
	window->redo = cloneUndoItems(this->redo);

	/* copy bookmarks */
	window->nMarks = this->nMarks;
	memcpy(&window->markTable, &this->markTable, sizeof(Bookmark) * window->nMarks);

	/* kick start the auto-indent engine */
	window->indentStyle = NO_AUTO_INDENT;
	window->SetAutoIndent(this->indentStyle);

	/* synchronize window state to this document */
	window->RefreshWindowStates();
}

/*
** Create a new editor window
*/
Document::Document(const char *name, char *geometry, bool iconic) {
	
	Widget winShell;
	Widget mainWin;
	Widget menuBar;
	Widget pane;
	Widget text;
	Widget stats;
	Widget statsAreaForm;
	Widget closeTabBtn;
	Widget tabForm;
	Pixel bgpix, fgpix;
	Arg al[20];
	int ac;
	XmString s1;
	XmFontList statsFontList;
	Document *win;
	char newGeometry[MAX_GEOM_STRING_LEN];
	unsigned int rows, cols;
	int x = 0, y = 0, bitmask, showTabBar, state;

	static Pixmap isrcFind = 0;
	static Pixmap isrcClear = 0;
	static Pixmap closeTabPixmap = 0;

	/* initialize window structure */
	/* + Schwarzenberg: should a
	  memset(window, 0, sizeof(Document));
	     be added here ?
	*/
	this->replaceDlog = nullptr;
	this->replaceText = nullptr;
	this->replaceWithText = nullptr;
	this->replaceWordToggle = nullptr;
	this->replaceCaseToggle = nullptr;
	this->replaceRegexToggle = nullptr;
	this->findDlog = nullptr;
	this->findText = nullptr;
	this->findWordToggle = nullptr;
	this->findCaseToggle = nullptr;
	this->findRegexToggle = nullptr;
	this->replaceMultiFileDlog = nullptr;
	this->replaceMultiFilePathBtn = nullptr;
	this->replaceMultiFileList = nullptr;
	this->multiFileReplSelected = FALSE;
	this->multiFileBusy = FALSE;
	this->writableWindows = nullptr;
	this->nWritableWindows = 0;
	this->fileChanged = FALSE;
	this->fileMode = 0;
	this->fileUid = 0;
	this->fileGid = 0;
	this->filenameSet = FALSE;
	this->fileFormat = UNIX_FILE_FORMAT;
	this->lastModTime = 0;
	this->fileMissing = True;
	strcpy(this->filename, name);
	this->undo = std::list<UndoInfo *>();
	this->redo = std::list<UndoInfo *>();
	this->nPanes = 0;
	this->autoSaveCharCount = 0;
	this->autoSaveOpCount = 0;
	this->undoMemUsed = 0;
	CLEAR_ALL_LOCKS(this->lockReasons);
	this->indentStyle = GetPrefAutoIndent(PLAIN_LANGUAGE_MODE);
	this->autoSave = GetPrefAutoSave();
	this->saveOldVersion = GetPrefSaveOldVersion();
	this->wrapMode = GetPrefWrap(PLAIN_LANGUAGE_MODE);
	this->overstrike = False;
	this->showMatchingStyle = GetPrefShowMatching();
	this->matchSyntaxBased = GetPrefMatchSyntaxBased();
	this->showStats = GetPrefStatsLine();
	this->showISearchLine = GetPrefISearchLine();
	this->showLineNumbers = GetPrefLineNums();
	this->highlightSyntax = GetPrefHighlightSyntax();
	this->backlightCharTypes = nullptr;
	this->backlightChars = GetPrefBacklightChars();
	if (this->backlightChars) {
		const char *cTypes = GetPrefBacklightCharTypes();
		if (cTypes && this->backlightChars) {
			this->backlightCharTypes = XtStringDup(cTypes);
		}
	}
	this->modeMessageDisplayed = FALSE;
	this->modeMessage = nullptr;
	this->ignoreModify = FALSE;
	this->windowMenuValid = FALSE;
	this->flashTimeoutID = 0;
	this->fileClosedAtom = None;
	this->wasSelected = FALSE;

	strcpy(this->fontName, GetPrefFontName());
	strcpy(this->italicFontName, GetPrefItalicFontName());
	strcpy(this->boldFontName, GetPrefBoldFontName());
	strcpy(this->boldItalicFontName, GetPrefBoldItalicFontName());
	this->colorDialog = nullptr;
	this->fontList = GetPrefFontList();
	this->italicFontStruct = GetPrefItalicFont();
	this->boldFontStruct = GetPrefBoldFont();
	this->boldItalicFontStruct = GetPrefBoldItalicFont();
	this->fontDialog = nullptr;
	this->nMarks = 0;
	this->markTimeoutID = 0;
	this->highlightData = nullptr;
	this->shellCmdData = nullptr;
	this->macroCmdData = nullptr;
	this->smartIndentData = nullptr;
	this->languageMode = PLAIN_LANGUAGE_MODE;
	this->iSearchHistIndex = 0;
	this->iSearchStartPos = -1;
	this->replaceLastRegexCase = TRUE;
	this->replaceLastLiteralCase = FALSE;
	this->iSearchLastRegexCase = TRUE;
	this->iSearchLastLiteralCase = FALSE;
	this->findLastRegexCase = TRUE;
	this->findLastLiteralCase = FALSE;
	this->tab = nullptr;
	this->device = 0;
	this->inode = 0;

	/* If window geometry was specified, split it apart into a window position
	   component and a window size component.  Create a new geometry string
	   containing the position component only.  Rows and cols are stripped off
	   because we can't easily calculate the size in pixels from them until the
	   whole window is put together.  Note that the preference resource is only
	   for clueless users who decide to specify the standard X geometry
	   application resource, which is pretty useless because width and height
	   are the same as the rows and cols preferences, and specifying a window
	   location will force all the windows to pile on top of one another */
	if (geometry == nullptr || geometry[0] == '\0')
		geometry = GetPrefGeometry();
	if (geometry == nullptr || geometry[0] == '\0') {
		rows = GetPrefRows();
		cols = GetPrefCols();
		newGeometry[0] = '\0';
	} else {
		bitmask = XParseGeometry(geometry, &x, &y, &cols, &rows);
		if (bitmask == 0)
			fprintf(stderr, "Bad window geometry specified: %s\n", geometry);
		else {
			if (!(bitmask & WidthValue))
				cols = GetPrefCols();
			if (!(bitmask & HeightValue))
				rows = GetPrefRows();
		}
		CreateGeometryString(newGeometry, x, y, 0, 0, bitmask & ~(WidthValue | HeightValue));
	}

	/* Create a new toplevel shell to hold the window */
	ac = 0;
	XtSetArg(al[ac], XmNtitle, name);
	ac++;
	XtSetArg(al[ac], XmNdeleteResponse, XmDO_NOTHING);
	ac++;
	XtSetArg(al[ac], XmNiconName, name);
	ac++;
	XtSetArg(al[ac], XmNgeometry, newGeometry[0] == '\0' ? nullptr : newGeometry);
	ac++;
	XtSetArg(al[ac], XmNinitialState, iconic ? IconicState : NormalState);
	ac++;

	if (newGeometry[0] == '\0') {
		/* Workaround to make Xt ignore Motif's bad PPosition size changes. Even
		   though we try to remove the PPosition in RealizeWithoutForcingPosition,
		   it is not sufficient.  Motif will recompute the size hints some point
		   later and put PPosition back! If the window is mapped after that time,
		   then the window will again wind up at 0, 0.  So, XEmacs does this, and
		   now we do.

		   Alternate approach, relying on ShellP.h:

		   ((WMShellWidget)winShell)->shell.client_specified &= ~_XtShellPPositionOK;
		 */

		XtSetArg(al[ac], XtNx, XT_IGNORE_PPOSITION);
		ac++;
		XtSetArg(al[ac], XtNy, XT_IGNORE_PPOSITION);
		ac++;
	}

	winShell = CreateWidget(TheAppShell, "textShell", topLevelShellWidgetClass, al, ac);
	this->shell = winShell;

#ifdef EDITRES
	XtAddEventHandler(winShell, (EventMask)0, True, (XtEventHandler)_XEditResCheckMessages, nullptr);
#endif /* EDITRES */

	addWindowIcon(winShell);

	/* Create a MainWindow to manage the menubar and text area, set the
	   userData resource to be used by WidgetToWindow to recover the
	   window pointer from the widget id of any of the window's widgets */
	XtSetArg(al[ac], XmNuserData, this);
	ac++;
	mainWin = XmCreateMainWindow(winShell, (String) "main", al, ac);
	this->mainWin = mainWin;
	XtManageChild(mainWin);

	/* The statsAreaForm holds the stats line and the I-Search line. */
	statsAreaForm = XtVaCreateWidget("statsAreaForm", xmFormWidgetClass, mainWin, XmNmarginWidth, STAT_SHADOW_THICKNESS, XmNmarginHeight, STAT_SHADOW_THICKNESS,
	                                 /* XmNautoUnmanage, False, */
	                                 nullptr);

	/* NOTE: due to a bug in openmotif 2.1.30, NEdit used to crash when
	   the i-search bar was active, and the i-search text widget was focussed,
	   and the window's width was resized to nearly zero.
	   In theory, it is possible to avoid this by imposing a minimum
	   width constraint on the nedit windows, but that width would have to
	   be at least 30 characters, which is probably unacceptable.
	   Amazingly, adding a top offset of 1 pixel to the toggle buttons of
	   the i-search bar, while keeping the the top offset of the text widget
	   to 0 seems to avoid avoid the crash. */

	this->iSearchForm = XtVaCreateWidget("iSearchForm", xmFormWidgetClass, statsAreaForm, XmNshadowThickness, 0, XmNleftAttachment, XmATTACH_FORM, XmNleftOffset, STAT_SHADOW_THICKNESS, XmNtopAttachment, XmATTACH_FORM, XmNtopOffset,
	                                       STAT_SHADOW_THICKNESS, XmNrightAttachment, XmATTACH_FORM, XmNrightOffset, STAT_SHADOW_THICKNESS, XmNbottomOffset, STAT_SHADOW_THICKNESS, nullptr);
	if (this->showISearchLine)
		XtManageChild(this->iSearchForm);

	/* Disable keyboard traversal of the find, clear and toggle buttons.  We
	   were doing this previously by forcing the keyboard focus back to the
	   text widget whenever a toggle changed.  That causes an ugly focus flash
	   on screen.  It's better just not to go there in the first place.
	   Plus, if the user really wants traversal, it's an X resource so it
	   can be enabled without too much pain and suffering. */

	if (isrcFind == 0) {
		isrcFind = createBitmapWithDepth(this->iSearchForm, (char *)isrcFind_bits, isrcFind_width, isrcFind_height);
	}
	this->iSearchFindButton = XtVaCreateManagedWidget("iSearchFindButton", xmPushButtonWidgetClass, this->iSearchForm, XmNlabelString, s1 = XmStringCreateSimpleEx("Find"), XmNlabelType, XmPIXMAP, XmNlabelPixmap, isrcFind,
	                                                    XmNtraversalOn, False, XmNmarginHeight, 1, XmNmarginWidth, 1, XmNleftAttachment, XmATTACH_FORM,
	                                                    /* XmNleftOffset, 3, */
	                                                    XmNleftOffset, 0, XmNtopAttachment, XmATTACH_FORM, XmNtopOffset, 1, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, 1, nullptr);
	XmStringFree(s1);

	this->iSearchCaseToggle = XtVaCreateManagedWidget("iSearchCaseToggle", xmToggleButtonWidgetClass, this->iSearchForm, XmNlabelString, s1 = XmStringCreateSimpleEx("Case"), XmNset,
	                                                    GetPrefSearch() == SEARCH_CASE_SENSE || GetPrefSearch() == SEARCH_REGEX || GetPrefSearch() == SEARCH_CASE_SENSE_WORD, XmNtopAttachment, XmATTACH_FORM, XmNbottomAttachment,
	                                                    XmATTACH_FORM, XmNtopOffset, 1, /* see openmotif note above */
	                                                    XmNrightAttachment, XmATTACH_FORM, XmNmarginHeight, 0, XmNtraversalOn, False, nullptr);
	XmStringFree(s1);

	this->iSearchRegexToggle =
	    XtVaCreateManagedWidget("iSearchREToggle", xmToggleButtonWidgetClass, this->iSearchForm, XmNlabelString, s1 = XmStringCreateSimpleEx("RegExp"), XmNset, GetPrefSearch() == SEARCH_REGEX_NOCASE || GetPrefSearch() == SEARCH_REGEX,
	                            XmNtopAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, XmNtopOffset, 1, /* see openmotif note above */
	                            XmNrightAttachment, XmATTACH_WIDGET, XmNrightWidget, this->iSearchCaseToggle, XmNmarginHeight, 0, XmNtraversalOn, False, nullptr);
	XmStringFree(s1);

	this->iSearchRevToggle = XtVaCreateManagedWidget("iSearchRevToggle", xmToggleButtonWidgetClass, this->iSearchForm, XmNlabelString, s1 = XmStringCreateSimpleEx("Rev"), XmNset, False, XmNtopAttachment, XmATTACH_FORM,
	                                                   XmNbottomAttachment, XmATTACH_FORM, XmNtopOffset, 1, /* see openmotif note above */
	                                                   XmNrightAttachment, XmATTACH_WIDGET, XmNrightWidget, this->iSearchRegexToggle, XmNmarginHeight, 0, XmNtraversalOn, False, nullptr);
	XmStringFree(s1);

	if (isrcClear == 0) {
		isrcClear = createBitmapWithDepth(this->iSearchForm, (char *)isrcClear_bits, isrcClear_width, isrcClear_height);
	}
	this->iSearchClearButton = XtVaCreateManagedWidget("iSearchClearButton", xmPushButtonWidgetClass, this->iSearchForm, XmNlabelString, s1 = XmStringCreateSimpleEx("<x"), XmNlabelType, XmPIXMAP, XmNlabelPixmap, isrcClear,
	                                                     XmNtraversalOn, False, XmNmarginHeight, 1, XmNmarginWidth, 1, XmNrightAttachment, XmATTACH_WIDGET, XmNrightWidget, this->iSearchRevToggle, XmNrightOffset, 2, XmNtopAttachment,
	                                                     XmATTACH_FORM, XmNtopOffset, 1, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, 1, nullptr);
	XmStringFree(s1);

	this->iSearchText = XtVaCreateManagedWidget("iSearchText", xmTextWidgetClass, this->iSearchForm, XmNmarginHeight, 1, XmNnavigationType, XmEXCLUSIVE_TAB_GROUP, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget,
	                                              this->iSearchFindButton, XmNrightAttachment, XmATTACH_WIDGET, XmNrightWidget, this->iSearchClearButton,
	                                              /* XmNrightOffset, 5, */
	                                              XmNtopAttachment, XmATTACH_FORM, XmNtopOffset, 0, /* see openmotif note above */
	                                              XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, 0, nullptr);
	RemapDeleteKey(this->iSearchText);

	SetISearchTextCallbacks(this);

	/* create the a form to house the tab bar and close-tab button */
	tabForm = XtVaCreateWidget("tabForm", xmFormWidgetClass, statsAreaForm, XmNmarginHeight, 0, XmNmarginWidth, 0, XmNspacing, 0, XmNresizable, False, XmNleftAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, XmNshadowThickness,
	                           0, nullptr);

	/* button to close top document */
	if (closeTabPixmap == 0) {
		closeTabPixmap = createBitmapWithDepth(tabForm, (char *)close_bits, close_width, close_height);
	}
	closeTabBtn = XtVaCreateManagedWidget("closeTabBtn", xmPushButtonWidgetClass, tabForm, XmNmarginHeight, 0, XmNmarginWidth, 0, XmNhighlightThickness, 0, XmNlabelType, XmPIXMAP, XmNlabelPixmap, closeTabPixmap, XmNshadowThickness, 1,
	                                      XmNtraversalOn, False, XmNrightAttachment, XmATTACH_FORM, XmNrightOffset, 3, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, 3, nullptr);
	XtAddCallback(closeTabBtn, XmNactivateCallback, closeTabCB, mainWin);

	/* create the tab bar */
	this->tabBar = XtVaCreateManagedWidget("tabBar", xmlFolderWidgetClass, tabForm, XmNresizePolicy, XmRESIZE_PACK, XmNleftAttachment, XmATTACH_FORM, XmNleftOffset, 0, XmNrightAttachment, XmATTACH_WIDGET, XmNrightWidget, closeTabBtn,
	                                         XmNrightOffset, 5, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, 0, XmNtopAttachment, XmATTACH_FORM, nullptr);

	this->tabMenuPane = CreateTabContextMenu(this->tabBar, this);
	AddTabContextMenuAction(this->tabBar);

	/* create an unmanaged composite widget to get the folder
	   widget to hide the 3D shadow for the manager area.
	   Note: this works only on the patched XmLFolder widget */
	Widget form = XtVaCreateWidget("form", xmFormWidgetClass, this->tabBar, XmNheight, 1, XmNresizable, False, nullptr);

	(void)form;

	XtAddCallback(this->tabBar, XmNactivateCallback, raiseTabCB, nullptr);

	this->tab = addTab(this->tabBar, name);

	/* A form to hold the stats line text and line/col widgets */
	this->statsLineForm = XtVaCreateWidget("statsLineForm", xmFormWidgetClass, statsAreaForm, XmNshadowThickness, 0, XmNtopAttachment, this->showISearchLine ? XmATTACH_WIDGET : XmATTACH_FORM, XmNtopWidget, this->iSearchForm,
	                                         XmNrightAttachment, XmATTACH_FORM, XmNleftAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, XmNresizable, False, /*  */
	                                         nullptr);

	/* A separate display of the line/column number */
	this->statsLineColNo = XtVaCreateManagedWidget("statsLineColNo", xmLabelWidgetClass, this->statsLineForm, XmNlabelString, s1 = XmStringCreateSimpleEx("L: ---  C: ---"), XmNshadowThickness, 0, XmNmarginHeight, 2, XmNtraversalOn,
	                                                 False, XmNtopAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, /*  */
	                                                 nullptr);
	XmStringFree(s1);

	/* Create file statistics display area.  Using a text widget rather than
	   a label solves a layout problem with the main window, which messes up
	   if the label is too long (we would need a resize callback to control
	   the length when the window changed size), and allows users to select
	   file names and line numbers.  Colors are copied from parent
	   widget, because many users and some system defaults color text
	   backgrounds differently from other widgets. */
	XtVaGetValues(this->statsLineForm, XmNbackground, &bgpix, nullptr);
	XtVaGetValues(this->statsLineForm, XmNforeground, &fgpix, nullptr);
	stats = XtVaCreateManagedWidget("statsLine", xmTextWidgetClass, this->statsLineForm, XmNbackground, bgpix, XmNforeground, fgpix, XmNshadowThickness, 0, XmNhighlightColor, bgpix, XmNhighlightThickness,
	                                0,                  /* must be zero, for OM (2.1.30) to
	                                                   aligns tatsLineColNo & statsLine */
	                                XmNmarginHeight, 1, /* == statsLineColNo.marginHeight - 1,
	                                                   to align with statsLineColNo */
	                                XmNscrollHorizontal, False, XmNeditMode, XmSINGLE_LINE_EDIT, XmNeditable, False, XmNtraversalOn, False, XmNcursorPositionVisible, False, XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,                /*  */
	                                XmNtopWidget, this->statsLineColNo, XmNleftAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_WIDGET, XmNrightWidget, this->statsLineColNo, XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET, /*  */
	                                XmNbottomWidget, this->statsLineColNo, XmNrightOffset, 3, nullptr);
	this->statsLine = stats;

	/* Give the statsLine the same font as the statsLineColNo */
	XtVaGetValues(this->statsLineColNo, XmNfontList, &statsFontList, nullptr);
	XtVaSetValues(this->statsLine, XmNfontList, statsFontList, nullptr);

	/* Manage the statsLineForm */
	if (this->showStats)
		XtManageChild(this->statsLineForm);

	/* If the fontList was nullptr, use the magical default provided by Motif,
	   since it must have worked if we've gotten this far */
	if (!this->fontList)
		XtVaGetValues(stats, XmNfontList, &this->fontList, nullptr);

	/* Create the menu bar */
	menuBar = CreateMenuBar(mainWin, this);
	this->menuBar = menuBar;
	XtManageChild(menuBar);

	/* Create paned window to manage split pane behavior */
	pane = XtVaCreateManagedWidget("pane", xmPanedWindowWidgetClass, mainWin, XmNseparatorOn, False, XmNspacing, 3, XmNsashIndent, -2, nullptr);
	this->splitPane = pane;
	XmMainWindowSetAreas(mainWin, menuBar, statsAreaForm, nullptr, nullptr, pane);

	/* Store a copy of document/window pointer in text pane to support
	   action procedures. See also WidgetToWindow() for info. */
	XtVaSetValues(pane, XmNuserData, this, nullptr);

	/* Patch around Motif's most idiotic "feature", that its menu accelerators
	   recognize Caps Lock and Num Lock as modifiers, and don't trigger if
	   they are engaged */
	AccelLockBugPatch(pane, this->menuBar);

	/* Create the first, and most permanent text area (other panes may
	   be added & removed, but this one will never be removed */
	text = createTextArea(pane, this, rows, cols, GetPrefEmTabDist(PLAIN_LANGUAGE_MODE), GetPrefDelimiters(), GetPrefWrapMargin(), this->showLineNumbers ? MIN_LINE_NUM_COLS : 0);
	XtManageChild(text);
	this->textArea = text;
	this->lastFocus = text;

	/* Set the initial colors from the globals. */
	this->SetColors(GetPrefColorName(TEXT_FG_COLOR), GetPrefColorName(TEXT_BG_COLOR), GetPrefColorName(SELECT_FG_COLOR), GetPrefColorName(SELECT_BG_COLOR), GetPrefColorName(HILITE_FG_COLOR), GetPrefColorName(HILITE_BG_COLOR),
	          GetPrefColorName(LINENO_FG_COLOR), GetPrefColorName(CURSOR_FG_COLOR));

	/* Create the right button popup menu (note: order is important here,
	   since the translation for popping up this menu was probably already
	   added in createTextArea, but CreateBGMenu requires this->textArea
	   to be set so it can attach the menu to it (because menu shells are
	   finicky about the kinds of widgets they are attached to)) */
	this->bgMenuPane = CreateBGMenu(this);

	/* cache user menus: init. user background menu cache */
	InitUserBGMenuCache(&this->userBGMenuCache);

	/* Create the text buffer rather than using the one created automatically
	   with the text area widget.  This is done so the syntax highlighting
	   modify callback can be called to synchronize the style buffer BEFORE
	   the text display's callback is called upon to display a modification */
	this->buffer = new TextBuffer;
	this->buffer->BufAddModifyCB(SyntaxHighlightModifyCB, this);

	/* Attach the buffer to the text widget, and add callbacks for modify */
	TextSetBuffer(text, this->buffer);
	this->buffer->BufAddModifyCB(modifiedCB, this);

	/* Designate the permanent text area as the owner for selections */
	HandleXSelections(text);

	/* Set the requested hardware tab distance and useTabs in the text buffer */
	this->buffer->BufSetTabDistance(GetPrefTabDist(PLAIN_LANGUAGE_MODE));
	this->buffer->useTabs_ = GetPrefInsertTabs();

	/* add the window to the global window list, update the Windows menus */
	this->addToWindowList();
	InvalidateWindowMenus();

	showTabBar = this->GetShowTabBar();
	if (showTabBar)
		XtManageChild(tabForm);

	manageToolBars(statsAreaForm);

	if (showTabBar || this->showISearchLine || this->showStats)
		XtManageChild(statsAreaForm);

	/* realize all of the widgets in the new window */
	RealizeWithoutForcingPosition(winShell);
	XmProcessTraversal(text, XmTRAVERSE_CURRENT);

	/* Make close command in window menu gracefully prompt for close */
	AddMotifCloseCallback(winShell, closeCB, this);

	/* Make window resizing work in nice character heights */
	this->UpdateWMSizeHints();

	/* Set the minimum pane height for the initial text pane */
	this->UpdateMinPaneHeights();

	/* create dialogs shared by all documents in a window */
	CreateFindDlog(this->shell, this);
	CreateReplaceDlog(this->shell, this);
	CreateReplaceMultiFileDlog(this);

	/* dim/undim Attach_Tab menu items */
	state = this->NDocuments() < NWindows();
	for (win = WindowList; win; win = win->next) {
		if (win->IsTopDocument()) {
			XtSetSensitive(win->moveDocumentItem, state);
			XtSetSensitive(win->contextMoveDocumentItem, state);
		}
	}
}

/*
** Create a new document in the shell window.
** Document are created in 'background' so that the user
** menus, ie. the Macro/Shell/BG menus, will not be updated
** unnecessarily; hence speeding up the process of opening
** multiple files.
*/
Document *Document::CreateDocument(const char *name) {
	Widget pane, text;
	int nCols, nRows;

	/* Allocate some memory for the new window data structure */
	auto window = new Document(*this);

#if 0
    /* share these dialog items with parent shell */
    window->replaceDlog = nullptr;
    window->replaceText = nullptr;
    window->replaceWithText = nullptr;
    window->replaceWordToggle = nullptr;
    window->replaceCaseToggle = nullptr;
    window->replaceRegexToggle = nullptr;
    window->findDlog = nullptr;
    window->findText = nullptr;
    window->findWordToggle = nullptr;
    window->findCaseToggle = nullptr;
    window->findRegexToggle = nullptr;
    window->replaceMultiFileDlog = nullptr;
    window->replaceMultiFilePathBtn = nullptr;
    window->replaceMultiFileList = nullptr;
    window->showLineNumbers = GetPrefLineNums();
    window->showStats = GetPrefStatsLine();
    window->showISearchLine = GetPrefISearchLine();
#endif

	window->multiFileReplSelected = FALSE;
	window->multiFileBusy = FALSE;
	window->writableWindows = nullptr;
	window->nWritableWindows = 0;
	window->fileChanged = FALSE;
	window->fileMissing = True;
	window->fileMode = 0;
	window->fileUid = 0;
	window->fileGid = 0;
	window->filenameSet = FALSE;
	window->fileFormat = UNIX_FILE_FORMAT;
	window->lastModTime = 0;
	strcpy(window->filename, name);
	window->undo = std::list<UndoInfo *>();
	window->redo = std::list<UndoInfo *>();
	window->nPanes = 0;
	window->autoSaveCharCount = 0;
	window->autoSaveOpCount = 0;
	window->undoMemUsed = 0;
	CLEAR_ALL_LOCKS(window->lockReasons);
	window->indentStyle = GetPrefAutoIndent(PLAIN_LANGUAGE_MODE);
	window->autoSave = GetPrefAutoSave();
	window->saveOldVersion = GetPrefSaveOldVersion();
	window->wrapMode = GetPrefWrap(PLAIN_LANGUAGE_MODE);
	window->overstrike = False;
	window->showMatchingStyle = GetPrefShowMatching();
	window->matchSyntaxBased = GetPrefMatchSyntaxBased();
	window->highlightSyntax = GetPrefHighlightSyntax();
	window->backlightCharTypes = nullptr;
	window->backlightChars = GetPrefBacklightChars();
	if (window->backlightChars) {
		const char *cTypes = GetPrefBacklightCharTypes();
		if (cTypes && window->backlightChars) {			
			window->backlightCharTypes = XtStringDup(cTypes);
		}
	}
	window->modeMessageDisplayed = FALSE;
	window->modeMessage = nullptr;
	window->ignoreModify = FALSE;
	window->windowMenuValid = FALSE;
	window->flashTimeoutID = 0;
	window->fileClosedAtom = None;
	window->wasSelected = FALSE;
	strcpy(window->fontName, GetPrefFontName());
	strcpy(window->italicFontName, GetPrefItalicFontName());
	strcpy(window->boldFontName, GetPrefBoldFontName());
	strcpy(window->boldItalicFontName, GetPrefBoldItalicFontName());
	window->colorDialog = nullptr;
	window->fontList = GetPrefFontList();
	window->italicFontStruct = GetPrefItalicFont();
	window->boldFontStruct = GetPrefBoldFont();
	window->boldItalicFontStruct = GetPrefBoldItalicFont();
	window->fontDialog = nullptr;
	window->nMarks = 0;
	window->markTimeoutID = 0;
	window->highlightData = nullptr;
	window->shellCmdData = nullptr;
	window->macroCmdData = nullptr;
	window->smartIndentData = nullptr;
	window->languageMode = PLAIN_LANGUAGE_MODE;
	window->iSearchHistIndex = 0;
	window->iSearchStartPos = -1;
	window->replaceLastRegexCase = TRUE;
	window->replaceLastLiteralCase = FALSE;
	window->iSearchLastRegexCase = TRUE;
	window->iSearchLastLiteralCase = FALSE;
	window->findLastRegexCase = TRUE;
	window->findLastLiteralCase = FALSE;
	window->tab = nullptr;
	window->bgMenuUndoItem = nullptr;
	window->bgMenuRedoItem = nullptr;
	window->device = 0;
	window->inode = 0;

	if (!window->fontList)
		XtVaGetValues(this->statsLine, XmNfontList, &window->fontList, nullptr);

	this->getTextPaneDimension(&nRows, &nCols);

	/* Create pane that actaully holds the new document. As
	   document is created in 'background', we need to hide
	   it. If we leave it unmanaged without setting it to
	   the XmNworkWindow of the mainWin, due to a unknown
	   bug in Motif where splitpane's scrollWindow child
	   somehow came up with a height taller than the splitpane,
	   the bottom part of the text editing widget is obstructed
	   when later brought up by  RaiseDocument(). So we first
	   manage it hidden, then unmanage it and reset XmNworkWindow,
	   then let RaiseDocument() show it later. */
	pane = XtVaCreateWidget("pane", xmPanedWindowWidgetClass, window->mainWin, XmNmarginWidth, 0, XmNmarginHeight, 0, XmNseparatorOn, False, XmNspacing, 3, XmNsashIndent, -2, XmNmappedWhenManaged, False, nullptr);
	XtVaSetValues(window->mainWin, XmNworkWindow, pane, nullptr);
	XtManageChild(pane);
	window->splitPane = pane;

	/* Store a copy of document/window pointer in text pane to support
	   action procedures. See also WidgetToWindow() for info. */
	XtVaSetValues(pane, XmNuserData, window, nullptr);

	/* Patch around Motif's most idiotic "feature", that its menu accelerators
	   recognize Caps Lock and Num Lock as modifiers, and don't trigger if
	   they are engaged */
	AccelLockBugPatch(pane, window->menuBar);

	/* Create the first, and most permanent text area (other panes may
	   be added & removed, but this one will never be removed */
	text = createTextArea(pane, window, nRows, nCols, GetPrefEmTabDist(PLAIN_LANGUAGE_MODE), GetPrefDelimiters(), GetPrefWrapMargin(), window->showLineNumbers ? MIN_LINE_NUM_COLS : 0);
	XtManageChild(text);
	window->textArea = text;
	window->lastFocus = text;

	/* Set the initial colors from the globals. */
	window->SetColors(GetPrefColorName(TEXT_FG_COLOR), GetPrefColorName(TEXT_BG_COLOR), GetPrefColorName(SELECT_FG_COLOR), GetPrefColorName(SELECT_BG_COLOR), GetPrefColorName(HILITE_FG_COLOR), GetPrefColorName(HILITE_BG_COLOR),
	          GetPrefColorName(LINENO_FG_COLOR), GetPrefColorName(CURSOR_FG_COLOR));

	/* Create the right button popup menu (note: order is important here,
	   since the translation for popping up this menu was probably already
	   added in createTextArea, but CreateBGMenu requires window->textArea
	   to be set so it can attach the menu to it (because menu shells are
	   finicky about the kinds of widgets they are attached to)) */
	window->bgMenuPane = CreateBGMenu(window);

	/* cache user menus: init. user background menu cache */
	InitUserBGMenuCache(&window->userBGMenuCache);

	/* Create the text buffer rather than using the one created automatically
	   with the text area widget.  This is done so the syntax highlighting
	   modify callback can be called to synchronize the style buffer BEFORE
	   the text display's callback is called upon to display a modification */
	window->buffer = new TextBuffer;
	window->buffer->BufAddModifyCB(SyntaxHighlightModifyCB, window);

	/* Attach the buffer to the text widget, and add callbacks for modify */
	TextSetBuffer(text, window->buffer);
	window->buffer->BufAddModifyCB(modifiedCB, window);

	/* Designate the permanent text area as the owner for selections */
	HandleXSelections(text);

	/* Set the requested hardware tab distance and useTabs in the text buffer */
	window->buffer->BufSetTabDistance(GetPrefTabDist(PLAIN_LANGUAGE_MODE));
	window->buffer->useTabs_ = GetPrefInsertTabs();
	window->tab = addTab(window->tabBar, name);

	/* add the window to the global window list, update the Windows menus */
	InvalidateWindowMenus();
	window->addToWindowList();

	/* return the shell ownership to previous tabbed doc */
	XtVaSetValues(window->mainWin, XmNworkWindow, this->splitPane, nullptr);
	XLowerWindow(TheDisplay, XtWindow(window->splitPane));
	XtUnmanageChild(window->splitPane);
	XtVaSetValues(window->splitPane, XmNmappedWhenManaged, True, nullptr);

	return window;
}