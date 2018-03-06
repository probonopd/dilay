/* This file is part of Dilay
 * Copyright © 2015-2018 Alexander Bau
 * Use and redistribute under the terms of the GNU General Public License
 */
#ifndef DILAY_SKETCH_BONE_INTERSECTION
#define DILAY_SKETCH_BONE_INTERSECTION

#include "sketch/fwd.hpp"
#include "sketch/mesh-intersection.hpp"

class SketchBoneIntersection : public SketchMeshIntersection
{
public:
  SketchBoneIntersection ();

  SketchNode&      parent () const;
  SketchNode&      child () const;
  const glm::vec3& projectedPosition () const;
  bool             update (float, const glm::vec3&, const glm::vec3&, const glm::vec3&, SketchMesh&,
                           SketchNode&);

private:
  SketchNode* _child;
  glm::vec3   _projectedPosition;
};

#endif
