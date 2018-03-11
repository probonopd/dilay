/* This file is part of Dilay
 * Copyright © 2015-2018 Alexander Bau
 * Use and redistribute under the terms of the GNU General Public License
 */
#include <QDesktopServices>
#include <QFileDialog>
#include <QMenuBar>
#include "../util.hpp"
#include "history.hpp"
#include "scene.hpp"
#include "state.hpp"
#include "tool/move-camera.hpp"
#include "view/configuration.hpp"
#include "view/floor-plane.hpp"
#include "view/gl-widget.hpp"
#include "view/info-pane.hpp"
#include "view/info-pane/scene.hpp"
#include "view/log.hpp"
#include "view/main-window.hpp"
#include "view/menu-bar.hpp"
#include "view/util.hpp"

namespace
{
  QAction& addAction (QMenu& menu, const QString& label, const QKeySequence& keySequence,
                      const std::function<void()>& f)
  {
    QAction* a = new QAction (label, &menu);
    a->setShortcut (keySequence);
    menu.addAction (a);
    QObject::connect (a, &QAction::triggered, f);
    return *a;
  }

  QAction& addCheckableAction (QMenu& menu, const QString& label, const QKeySequence& keySequence,
                               bool state, const std::function<void(bool)>& f)
  {
    QAction* a = new QAction (label, &menu);
    a->setShortcut (keySequence);
    a->setCheckable (true);
    a->setChecked (state);
    menu.addAction (a);
    QObject::connect (a, &QAction::toggled, f);
    return *a;
  }

  QString getFileDialogPath (const Scene& scene)
  {
    return scene.hasFileName ()
             ? QString (scene.fileName ().c_str ())
             : QStandardPaths::standardLocations (QStandardPaths::HomeLocation).front ();
  }

  QString filterAllFiles () { return QObject::tr ("All files (*.*)"); }

  QString filterDlyFiles () { return QObject::tr ("Dilay files (*.dly)"); }

  QString filterObjFiles () { return QObject::tr ("Wavefront files (*.obj)"); }

  QString fileDialogFilters ()
  {
    return filterAllFiles () + ";;" + filterDlyFiles () + ";;" + filterObjFiles ();
  }

  QString selectedFilter (const Scene& scene)
  {
    if (scene.hasFileName ())
    {
      if (Util::hasSuffix (scene.fileName (), ".dly"))
      {
        return filterDlyFiles ();
      }
      else if (Util::hasSuffix (scene.fileName (), ".obj"))
      {
        return filterObjFiles ();
      }
    }
    return filterAllFiles ();
  }
}

void ViewMenuBar::setup (ViewMainWindow& mainWindow, ViewGlWidget& glWidget)
{
  QMenuBar& menuBar = *mainWindow.menuBar ();
  QMenu&    fileMenu = *menuBar.addMenu (QObject::tr ("&File"));
  QMenu&    editMenu = *menuBar.addMenu (QObject::tr ("&Edit"));
  QMenu&    viewMenu = *menuBar.addMenu (QObject::tr ("&View"));
  QMenu&    helpMenu = *menuBar.addMenu (QObject::tr ("&Help"));

  addAction (fileMenu, QObject::tr ("&Open..."), QKeySequence::Open, [&mainWindow, &glWidget]() {
    Scene&            scene = glWidget.state ().scene ();
    QString           filter = filterAllFiles ();
    const std::string fileName =
      QFileDialog::getOpenFileName (&mainWindow, QObject::tr ("Open"), getFileDialogPath (scene),
                                    fileDialogFilters (), &filter, QFileDialog::DontUseNativeDialog)
        .toStdString ();
    if (fileName.empty () == false)
    {
#ifndef NDEBUG
      scene.reset ();
      glWidget.state ().history ().reset ();
#else
      if (scene.isEmpty () == false) {
        if (ViewUtil::question (mainWindow, QObject::tr ("Replace existent scene?"))) {
          scene.reset ();
          glWidget.state ().history ().reset ();
        }
        else {
          glWidget.state ().history ().snapshotAll (scene);
        }
      }
#endif
      if (scene.fromDlyFile (glWidget.state ().config (), fileName) == false)
      {
        ViewUtil::error (mainWindow, QObject::tr ("Could not open file."));
      }
      mainWindow.infoPane ().scene ().updateInfo ();
      mainWindow.update ();
    }
  });

  QAction& saveAsAction = addAction (
    fileMenu, QObject::tr ("Save &as..."), QKeySequence::SaveAs, [&mainWindow, &glWidget]() {
      Scene&            scene = glWidget.state ().scene ();
      QString           filter = selectedFilter (scene);
      const std::string fileName =
        QFileDialog::getSaveFileName (&mainWindow, QObject::tr ("Save as"),
                                      getFileDialogPath (scene), fileDialogFilters (), &filter,
                                      QFileDialog::DontUseNativeDialog)
          .toStdString ();
      if (fileName.empty () == false)
      {
        const bool saveAsObj = Util::hasSuffix (fileName, ".obj") || filter == filterObjFiles ();

        if (scene.toDlyFile (fileName, saveAsObj) == false)
        {
          ViewUtil::error (mainWindow, QObject::tr ("Could not save to file."));
        }
        else if (saveAsObj && scene.numSketchMeshes () > 0)
        {
          ViewUtil::info (mainWindow,
                          QObject::tr ("Sketches are omitted when saving Wavefront files."));
        }
      }
    });

  addAction (fileMenu, QObject::tr ("&Save"), QKeySequence::Save,
             [&mainWindow, &glWidget, &saveAsAction]() {
               Scene& scene = glWidget.state ().scene ();
               if (scene.hasFileName ())
               {
                 const bool saveAsObj = Util::hasSuffix (scene.fileName (), ".obj");

                 if (scene.toDlyFile (saveAsObj) == false)
                 {
                   ViewUtil::error (mainWindow, QObject::tr ("Could not save to file."));
                 }
               }
               else
               {
                 saveAsAction.trigger ();
               }
             });

  fileMenu.addSeparator ();

  addAction (fileMenu, QObject::tr ("&Quit"), QKeySequence::Quit,
             [&mainWindow]() { mainWindow.close (); });
  addAction (editMenu, QObject::tr ("&Undo"), QKeySequence::Undo,
             [&glWidget]() { glWidget.state ().undo (); });
  addAction (editMenu, QObject::tr ("&Redo"), QKeySequence::Redo,
             [&glWidget]() { glWidget.state ().redo (); });
  addAction (editMenu, QObject::tr ("&Configuration..."), QKeySequence (),
             [&mainWindow, &glWidget]() { ViewConfiguration::show (mainWindow, glWidget); });

  addAction (viewMenu, QObject::tr ("Toggle &info pane"), Qt::Key_I, [&mainWindow]() {
    if (mainWindow.infoPane ().isVisible ())
    {
      mainWindow.infoPane ().close ();
    }
    else
    {
      mainWindow.infoPane ().show ();
    }
  });

  viewMenu.addSeparator ();

  addAction (viewMenu, QObject::tr ("&Snap camera"), Qt::SHIFT + Qt::Key_C,
             [&glWidget]() { glWidget.toolMoveCamera ().snap (glWidget.state (), false); });
  addAction (viewMenu, QObject::tr ("Reset &gaze point"), Qt::ALT + Qt::Key_C,
             [&glWidget]() { glWidget.toolMoveCamera ().resetGazePoint (glWidget.state ()); });

  viewMenu.addSeparator ();

  addAction (viewMenu, QObject::tr ("Toggle &wireframe"), Qt::Key_W, [&mainWindow, &glWidget]() {
    glWidget.state ().scene ().toggleWireframe ();
    mainWindow.update ();
  });

  addAction (viewMenu, QObject::tr ("Toggle &shading"), Qt::SHIFT + Qt::Key_W,
             [&mainWindow, &glWidget]() {
               glWidget.state ().scene ().toggleShading ();
               mainWindow.update ();
             });

  addCheckableAction (viewMenu, QObject::tr ("Show &floor plane"), QKeySequence (), false,
                      [&mainWindow, &glWidget](bool a) {
                        glWidget.floorPlane ().isActive (a);
                        mainWindow.update ();
                      });

  addAction (helpMenu, QObject::tr ("&Manual..."), QKeySequence (), [&mainWindow]() {
    if (QDesktopServices::openUrl (QUrl ("http://abau.org/dilay/manual.html")) == false)
    {
      ViewUtil::error (mainWindow, QObject::tr ("Could not open manual."));
    }
  });

  addAction (helpMenu, QObject::tr ("&View log..."), QKeySequence (),
             [&mainWindow]() { ViewLog::show (mainWindow); });

  addAction (helpMenu, QObject::tr ("&About Dilay..."), QKeySequence (), [&mainWindow]() {
    ViewUtil::about (
      mainWindow,
      QString ("Dilay " DILAY_VERSION " - ") + QObject::tr ("a 3D sculpting application") +
        QString ("\n\n") + QString ("Copyright © 2015-2018 Alexander Bau") + QString ("\n\n") +
        QObject::tr ("Use and redistribute under the terms of the GNU General Public License"));
  });
}
