// SPDX-License-Identifier: GPL-2.0
#include "printer.h"
#include "templatelayout.h"
#include "core/divelist.h"
#include "core/divelog.h"
#include "core/selection.h"
#include "core/statistics.h"
#include "core/qthelper.h"
#include "profile-widget/profilescene.h"

#include <algorithm>
#include <memory>
#include <QImage>
#include <QPainter>
#include <QPrinter>
#include <QtWebKitWidgets>
#include <QWebElementCollection>
#include <QWebElement>
#include <iostream>

Printer::Printer(QPaintDevice *paintDevice, const print_options &printOptions, const template_options &templateOptions, PrintMode printMode, dive *singleDive) :
	paintDevice(paintDevice),
	webView(new QWebView),
	printOptions(printOptions),
	templateOptions(templateOptions),
	printMode(printMode),
	singleDive(singleDive),
	done(0)
{
	connect(webView, SIGNAL(loadFinished(bool)), this, SLOT(loadFinished(bool)));
}

Printer::~Printer()
{
	delete webView;
}

QString Printer::getProfileImage(int width, int height, const dive *d, bool isGrayscale)
{
	ProfileScene profile(1.25, true, isGrayscale);
	QRect pos(0, 0, width / 1.25, height / 1.25); // XXX !!!
	QImage image = profile.draw(pos, d, 0, nullptr, false);
	QByteArray bytearray;
	QBuffer buffer(&bytearray);
	buffer.open(QIODevice::WriteOnly);
	image.save(&buffer, "JPEG");
	buffer.close();
	QString html = "data:image/jpeg;base64,";
	html.append(bytearray.toBase64());
	return html;
}

//value: ranges from 0 : 100 and shows the progress of the templating engine
void Printer::templateProgessUpdated(int value)
{
	done = value / 5; //template progess if 1/5 of total work
	emit progessUpdated(done);
}

std::vector<dive *> Printer::getDives() const
{
	if (singleDive) {
		return { singleDive };
	} else if (printOptions.print_selected) {
		return getDiveSelection();
	} else {
		std::vector<dive *> res;
		for (auto &d: divelog.dives)
			res.push_back(d.get());
		return res;
	}
}

QString Printer::exportHtml()
{
	TemplateLayout t(printOptions, templateOptions);
	connect(&t, SIGNAL(progressUpdated(int)), this, SLOT(templateProgessUpdated(int)));
	QString html;

	if (printOptions.type == print_options::DIVELIST)
		html = t.generate(getDives());
	else if (printOptions.type == print_options::STATISTICS )
		html = t.generateStatistics();

	// TODO: write html to file
	return html;
}

void Printer::print()
{
	QPrinter *printerPtr;
	printerPtr = static_cast<QPrinter*>(paintDevice);

	TemplateLayout t(printOptions, templateOptions);
	connect(&t, SIGNAL(progressUpdated(int)), this, SLOT(templateProgessUpdated(int)));

	// export border width with at least 1 pixel
	// templateOptions.borderwidth = std::max(1, pageSize.width() / 1000);
	mainLoadFinished = false;
	imageLoadsPending = 0;

	webView->resize(794 * 1.25, 1123 * 1.25); // XXX A4 at 96 dpi is 794/1123

	if (printOptions.type == print_options::DIVELIST)
		webView->setHtml(t.generate(getDives()));
	else if (printOptions.type == print_options::STATISTICS )
		webView->setHtml(t.generateStatistics());
	if (printOptions.color_selected && printerPtr->colorMode())
		printerPtr->setColorMode(QPrinter::Color);
	else
		printerPtr->setColorMode(QPrinter::GrayScale);

	if (!mainLoadFinished)
		printWaiter.exec();
}

void Printer::loadFinished(bool ok)
{
	if (!ok) {
		mainLoadFinished = true;
		printWaiter.quit();
		return;
	}

	imageLoadsPending++;

	webView->page()->mainFrame()->addToJavaScriptWindowObject("ssrfCallback", this);

	webView->page()->mainFrame()->evaluateJavaScript(
		"(function() {"
			"var printStyle = document.createElement('style');"
			"printStyle.id = 'printStyle';"
			"printStyle.innerHTML = "
				"'body.simulate-print {' +"
					"'width: 210mm;' +"
					"'height: 297mm;' +"
					"'margin: 20mm;' +"
				"'}' +"
				"'@page {' +"
					"'size: A4;' +"
					"'margin: 20mm;' +"
				"'}';"
			"document.head.appendChild(printStyle);"
			"document.body.classList.add('simulate-print');"
		"})()"
	);

	const auto &result = webView->page()->mainFrame()->evaluateJavaScript(
		"(function() {"
			"var elements = document.getElementsByClassName('DiveProfile');"
			"var ret = [];"
			"for (var i = 0; i < elements.length; i++) {"
				"var ele = elements[i];"
				"ret.push({"
					"id: ele.id,"
					"width: ele.offsetWidth,"
					"height: ele.offsetHeight,"
				"});"
			"}"
			"return ret;"
		"})()"
	);
	const auto &list = result.toList();
	for (auto it = list.cbegin(); it != list.cend(); it++) {
		const auto &ele = *it;
		if (!ele.isValid())
			continue;
		const auto &map = ele.toMap();
		const QString &elementId = map[QString("id")].toString();
		QString diveIdString = elementId;
		if (!diveIdString.startsWith("dive_"))
			continue;
		int diveId = diveIdString.remove(0, 5).toInt(0, 10);
		struct dive *dive = divelog.dives.get_by_uniq_id(diveId);

		int width = map[QString("width")].toInt();
		int height = map[QString("height")].toInt();

		width *= printOptions.resolution;
		height *= printOptions.resolution;
		width /= 96;
		height /= 96;

		QString profileImage = getProfileImage(width, height, dive, !printOptions.color_selected);
		//std::cerr << "image " << profileImage.toStdString() << "\n";

		imageLoadsPending++;

		webView->page()->mainFrame()->evaluateJavaScript(
			"(function() {"
				"var ele = document.getElementById('" + elementId + "');"
				"var img = document.createElement('img');"
				"img.src = '" + profileImage + "';"
				"img.onload = function() {"
					"window.ssrfCallback.imageLoadFinished();"
				"};"
				"while (ele.firstChild)"
					"ele.removeChild(ele.firstChild);"
				"ele.appendChild(img);"
			"})();"
		);
	}

	webView->page()->mainFrame()->evaluateJavaScript(
		"(function() {"
			"document.body.classList.remove('simulate-print');"
			"var printStyle = document.getElementById('printStyle');"
			"printStyle.remove();"
		"})()"
	);

	emit imageLoadFinished();
}

void Printer::imageLoadFinished()
{
	imageLoadsPending--;
	if (imageLoadsPending)
		return;

	mainLoadFinished = true;

	webView->print(static_cast<QPrinter*>(paintDevice));

	printWaiter.quit();
}

void Printer::previewOnePage()
{
	if (printMode == PREVIEW) {
		TemplateLayout t(printOptions, templateOptions);

		pageSize.setHeight(paintDevice->height());
		pageSize.setWidth(paintDevice->width());
		webView->page()->setViewportSize(pageSize);
		// initialize the border settings
		// templateOptions.border_width = std::max(1, pageSize.width() / 1000);
		if (printOptions.type == print_options::DIVELIST)
			webView->setHtml(t.generate(getDives()));
		else if (printOptions.type == print_options::STATISTICS )
			webView->setHtml(t.generateStatistics());
	}
}
