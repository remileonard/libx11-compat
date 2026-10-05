/*
 * Small Open Inventor program built against libx11-compat: three lit shapes on
 * a slowly spinning turntable, shown in the Motif SoXtExaminerViewer.
 *
 * Build and run (Linux, GLX=1):
 *   make GLX=1 open-inventor-examples
 *   scripts/run-open-inventor.sh inventor-shapes
 *
 * Any other examples/inventor/<name>.c++ builds the same way.
 */
#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/engines/SoElapsedTime.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoRotationXYZ.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoTranslation.h>

static SoSeparator *makeShape(SoNode *shape, float x, float r, float g, float b)
{
    SoSeparator *sep = new SoSeparator;
    SoTranslation *at = new SoTranslation;
    at->translation.setValue(x, 0.0f, 0.0f);
    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(r, g, b);
    mat->specularColor.setValue(0.3f, 0.3f, 0.3f);
    mat->shininess = 0.5f;
    sep->addChild(at);
    sep->addChild(mat);
    sep->addChild(shape);
    return sep;
}

int main(int, char **argv)
{
    Widget window = SoXt::init(argv[0]);
    if (window == NULL)
        return 1;

    SoSeparator *root = new SoSeparator;
    root->ref();
    root->addChild(new SoPerspectiveCamera);
    root->addChild(new SoDirectionalLight);

    // Spin everything below about Y, driven by the elapsed-time engine.
    SoRotationXYZ *spin = new SoRotationXYZ;
    spin->axis = SoRotationXYZ::Y;
    SoElapsedTime *clock = new SoElapsedTime;
    clock->speed = 0.4f;
    spin->angle.connectFrom(&clock->timeOut);
    root->addChild(spin);

    root->addChild(makeShape(new SoCube, -3.0f, 0.9f, 0.3f, 0.2f));
    root->addChild(makeShape(new SoSphere, 0.0f, 0.2f, 0.7f, 0.3f));
    root->addChild(makeShape(new SoCone, 3.0f, 0.2f, 0.4f, 0.9f));

    SoXtExaminerViewer *viewer = new SoXtExaminerViewer(window);
    viewer->setSceneGraph(root);
    viewer->setTitle("libx11-compat: Open Inventor shapes");
    viewer->viewAll();
    viewer->show();

    SoXt::show(window);
    SoXt::mainLoop();
    return 0;
}
