// Runs the QML surface tests in this directory.
//
// These exercise what a control puts on screen, which the C++ controller
// suites cannot observe: they prove the catalog holds the right value, not
// that the control displays it.
#include <QtQuickTest/quicktest.h>

QUICK_TEST_MAIN(northstar_qml_surfaces)
