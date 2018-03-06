/* This file is part of Dilay
 * Copyright © 2015-2018 Alexander Bau
 * Use and redistribute under the terms of the GNU General Public License
 */
#ifndef DILAY_HISTORY
#define DILAY_HISTORY

#include <functional>
#include "configurable.hpp"
#include "macro.hpp"

class DynamicMesh;
class Scene;
class State;

class History : public Configurable
{
public:
  DECLARE_BIG3 (History, const Config&)

  void snapshotAll (const Scene&);
  void snapshotDynamicMeshes (const Scene&);
  void snapshotSketchMeshes (const Scene&);
  void dropPastSnapshot ();
  void dropFutureSnapshot ();
  void undo (State&);
  void redo (State&);
  bool hasRecentDynamicMesh () const;
  void forEachRecentDynamicMesh (const std::function<void(const DynamicMesh&)>&) const;
  void reset ();

private:
  IMPLEMENTATION

  void runFromConfig (const Config&);
};

#endif
