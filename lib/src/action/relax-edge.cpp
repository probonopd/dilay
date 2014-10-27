#include <glm/glm.hpp>
#include "action/relax-edge.hpp"
#include "action/unit/on.hpp"
#include "affected-faces.hpp"
#include "partial-action/flip-edge.hpp"
#include "winged/edge.hpp"
#include "winged/vertex.hpp"

struct ActionRelaxEdge::Impl {
  ActionUnitOn <WingedMesh> actions;

  void runUndo (WingedMesh& mesh) { this->actions.undo (mesh); }
  void runRedo (WingedMesh& mesh) { this->actions.redo (mesh); }

  void run (WingedMesh& mesh, WingedEdge& edge, AffectedFaces& affectedFaces) {
    if (this->relaxableEdge (edge)) {
      affectedFaces.insert (edge.leftFaceRef  ());
      affectedFaces.insert (edge.rightFaceRef ());
      this->actions.add <PAFlipEdge> ().run (mesh, edge);
    }
  }

  bool relaxableEdge (const WingedEdge& edge) const {
    const int v1  = int (edge.vertex1Ref ().valence ());
    const int v2  = int (edge.vertex2Ref ().valence ());
    const int v3  = int (edge.vertexRef (edge.leftFaceRef  (), 2).valence ());
    const int v4  = int (edge.vertexRef (edge.rightFaceRef (), 2).valence ());

    const int pre  = glm::abs (v1-6)   + glm::abs (v2-6)   + glm::abs (v3-6)   + glm::abs (v4-6);
    const int post = glm::abs (v1-6-1) + glm::abs (v2-6-1) + glm::abs (v3-6+1) + glm::abs (v4-6+1);

    return post < pre;
  }
};

DELEGATE_BIG3 (ActionRelaxEdge)
DELEGATE3 (void, ActionRelaxEdge, run, WingedMesh&, WingedEdge&, AffectedFaces&)
DELEGATE1 (void, ActionRelaxEdge, runUndo, WingedMesh&)
DELEGATE1 (void, ActionRelaxEdge, runRedo, WingedMesh&)
