// SPDX-License-Identifier: GPL-2.0
#ifndef PRINTER_H
#define PRINTER_H

#include "printoptions.h"
#include "templateedit.h"

struct dive;
class ProfileScene;
class QPainter;
class QPaintDevice;
class QRect;
class QWebElement;
class QWebView;

class Printer : public QObject {
	Q_OBJECT

public:
	enum PrintMode {
		PRINT,
		PREVIEW
	};

private:
	QPaintDevice *paintDevice;
	QWebView *webView;
	const print_options &printOptions;
	const template_options &templateOptions;
	QSize pageSize;
	PrintMode printMode;
	struct dive *singleDive;
	int done;
	std::vector<dive *> getDives() const;
	QString getProfileImage(int width, int height, const dive *d, bool isGrayscale);
	bool mainLoadFinished;
	QEventLoop printWaiter;
	int imageLoadsPending;

private slots:
	void templateProgessUpdated(int value);
	void loadFinished(bool ok);

public:
	// If singleDive is non-null, then only print this particular dive.
	Printer(QPaintDevice *paintDevice, const print_options &printOptions, const template_options &templateOptions,
		PrintMode printMode, dive *singleDive);
	~Printer();
	void print();
	void previewOnePage();
	QString exportHtml();

public slots:
	void imageLoadFinished();

signals:
	void progessUpdated(int value);
};

#endif //PRINTER_H
