
#ifndef SEARCH_H_
#define SEARCH_H_

#include "Direction.h"
#include "SearchType.h"
#include "WrapMode.h"
#include "util/string_view.h"
#include <QString>
#include <memory>

class DocumentWidget;
class MainWindow;
class TextArea;
class Regex;

// Maximum length of search string history
constexpr int MAX_SEARCH_HISTORY = 100;

bool isRegexType(SearchType searchType);
bool replaceUsingREEx(const QString &searchStr, const QString &replaceStr, view::string_view sourceStr, int beginPos, std::string &dest, int prevChar, const QString &delimiters, int defaultFlags);
bool replaceUsingREEx(view::string_view searchStr, const char *replaceStr, view::string_view sourceStr, int beginPos, std::string &dest, int prevChar, const char *delimiters, int defaultFlags);
bool SearchString(view::string_view string, const QString &searchString, Direction direction, SearchType searchType, WrapMode wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW, const QString &delimiters);
int countWritableWindows();
int defaultRegexFlags(SearchType searchType);
int historyIndex(int nCycles);
std::string ReplaceAllInStringEx(view::string_view inString, const QString &searchString, const QString &replaceString, SearchType searchType, int *copyStart, int *copyEnd, const QString &delimiters, bool *ok);
void saveSearchHistory(const QString &searchString, QString replaceString, SearchType searchType, bool isIncremental);

std::unique_ptr<Regex> make_regex(const QString &re, int flags);

struct SearchReplaceHistoryEntry {
    QString    search;
    QString    replace;
    SearchType type;
};

extern int NHist;
extern SearchReplaceHistoryEntry SearchReplaceHistory[MAX_SEARCH_HISTORY];

#endif
