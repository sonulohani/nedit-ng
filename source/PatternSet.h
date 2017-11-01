
#ifndef PATTERN_SET_H_
#define PATTERN_SET_H_

#include "HighlightPattern.h"
#include <QString>
#include <QVector>

// Header for a set of patterns
class PatternSet {
public:
	bool operator!=(const PatternSet &rhs) const;
	bool operator==(const PatternSet &rhs) const;


public:
	QString                   languageMode;
    int                       lineContext = 0;
    int                       charContext = 0;
	QVector<HighlightPattern> patterns;
};

#endif
