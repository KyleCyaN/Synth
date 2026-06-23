#include "InjectorGUI.h"
#include <QApplication>

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);

	app.setApplicationName("Synth");
	app.setOrganizationName("Synth");
	InjectorGUI window;
	window.setWindowTitle("Synth");
	window.setFixedSize(400, 200);
	window.show();

	return QApplication::exec();
}