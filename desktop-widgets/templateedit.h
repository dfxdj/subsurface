// SPDX-License-Identifier: GPL-2.0
#ifndef TEMPLATEEDIT_H
#define TEMPLATEEDIT_H

#include <QDialog>
#include "printoptions.h"

class QPrinter;

namespace Ui {
class TemplateEdit;
}

class TemplateEdit : public QDialog
{
	Q_OBJECT

public:
	explicit TemplateEdit(QWidget *parent, const print_options &printOptions, template_options &templateOptions);
	~TemplateEdit();
private slots:
	void on_fontsize_valueChanged(int font_size);
	void on_linespacing_valueChanged(double line_spacing);
	void on_borderwidth_valueChanged(double border_width);
	void on_fontSelection_currentIndexChanged(int index);
	void on_colorpalette_currentIndexChanged(int index);
	void on_buttonBox_clicked(QAbstractButton *button);
	void colorSelect(QAbstractButton *button);
	void onPaintRequested(QPrinter *);

private:
	Ui::TemplateEdit *ui;
	QButtonGroup *btnGroup;
	bool editingCustomColors;
	const print_options &printOptions;
	template_options &templateOptions;
	struct template_options newTemplateOptions;
	QString grantlee_template;
	void saveSettings();
	void updatePreview();
	bool previewBusy = false;

};

#endif // TEMPLATEEDIT_H
