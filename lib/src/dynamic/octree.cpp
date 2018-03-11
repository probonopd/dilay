/* This file is part of Dilay
 * Copyright © 2015-2018 Alexander Bau
 * Use and redistribute under the terms of the GNU General Public License
 */
#include <array>
#include <functional>
#include <glm/glm.hpp>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include "dynamic/octree.hpp"
#include "intersection.hpp"
#include "maybe.hpp"
#include "primitive/aabox.hpp"
#include "primitive/plane.hpp"
#include "primitive/sphere.hpp"
#include "util.hpp"

#ifdef DILAY_RENDER_OCTREE
#include "../mesh.hpp"
#include "color.hpp"
#include "render-mode.hpp"
#endif

namespace
{
  struct IndexOctreeNode;
  typedef Maybe<IndexOctreeNode> Child;

  struct IndexOctreeStatistics
  {
    typedef std::unordered_map<int, unsigned int> DepthMap;

    unsigned int numNodes;
    unsigned int numElements;
    int          minDepth;
    int          maxDepth;
    unsigned int maxElementsPerNode;
    DepthMap     numElementsPerDepth;
    DepthMap     numNodesPerDepth;
  };

  struct IndexOctreeNode
  {
    const glm::vec3                  center;
    const float                      width;
    const int                        depth;
    const PrimAABox                  looseAABox;
    std::array<Child, 8>             children;
    std::unordered_set<unsigned int> indices;

    static constexpr float relativeMinElementExtent = 0.25f;

    IndexOctreeNode (const glm::vec3& c, float w, int d)
      : center (c)
      , width (w)
      , depth (d)
      , looseAABox (c, 2.0f * w, 2.0f * w, 2.0f * w)
    {
      static_assert (IndexOctreeNode::relativeMinElementExtent < 0.5f,
                     "relativeMinElementExtent must be smaller than 0.5f");
      assert (w > 0.0f);
    }

    bool approxContains (const glm::vec3& position, float maxDimExtent) const
    {
      const glm::vec3 min = this->center - glm::vec3 (Util::epsilon () + (this->width * 0.5f));
      const glm::vec3 max = this->center + glm::vec3 (Util::epsilon () + (this->width * 0.5f));
      return glm::all (glm::lessThanEqual (min, position)) &&
             glm::all (glm::lessThanEqual (position, max)) && maxDimExtent <= this->width;
    }

    /* node indices:
     *   (-,-,-) -> 0
     *   (-,-,+) -> 1
     *   (-,+,-) -> 2
     *   (-,+,+) -> 3
     *   (+,-,-) -> 4
     *   (+,-,+) -> 5
     *   (+,+,-) -> 6
     *   (+,+,+) -> 7
     */
    unsigned int childIndex (const glm::vec3& position) const
    {
      unsigned int index = 0;
      if (this->center.x < position.x)
      {
        index += 4;
      }
      if (this->center.y < position.y)
      {
        index += 2;
      }
      if (this->center.z < position.z)
      {
        index += 1;
      }
      return index;
    }

    bool hasChildren () const
    {
      return bool(this->children[0]) || bool(this->children[1]) || bool(this->children[2]) ||
             bool(this->children[3]) || bool(this->children[4]) || bool(this->children[5]) ||
             bool(this->children[6]) || bool(this->children[7]);
    }

    bool insertIntoChild (float maxDimExtent) const
    {
      return maxDimExtent <= this->width * IndexOctreeNode::relativeMinElementExtent;
    }

    IndexOctreeNode& insertIntoChild (unsigned int index, const glm::vec3& position,
                                      float maxDimExtent)
    {
      const unsigned int childIndex = this->childIndex (position);

      if (this->children[childIndex] == false)
      {
        const float q = this->width * 0.25f;
        const float width = this->width * 0.5f;
        const int   depth = this->depth + 1;

        switch (childIndex)
        {
          case 0:
            this->children[0] = Child::make (this->center + glm::vec3 (-q, -q, -q), width, depth);
            break;
          case 1:
            this->children[1] = Child::make (this->center + glm::vec3 (-q, -q, +q), width, depth);
            break;
          case 2:
            this->children[2] = Child::make (this->center + glm::vec3 (-q, +q, -q), width, depth);
            break;
          case 3:
            this->children[3] = Child::make (this->center + glm::vec3 (-q, +q, +q), width, depth);
            break;
          case 4:
            this->children[4] = Child::make (this->center + glm::vec3 (+q, -q, -q), width, depth);
            break;
          case 5:
            this->children[5] = Child::make (this->center + glm::vec3 (+q, -q, +q), width, depth);
            break;
          case 6:
            this->children[6] = Child::make (this->center + glm::vec3 (+q, +q, -q), width, depth);
            break;
          case 7:
            this->children[7] = Child::make (this->center + glm::vec3 (+q, +q, +q), width, depth);
            break;
        }
      }
      return this->children[childIndex]->addElement (index, position, maxDimExtent);
    }

    IndexOctreeNode& addElement (unsigned int index, const glm::vec3& position, float maxDimExtent)
    {
      assert (this->approxContains (position, maxDimExtent));

      if (this->insertIntoChild (maxDimExtent))
      {
        return this->insertIntoChild (index, position, maxDimExtent);
      }
      else
      {
        this->indices.insert (index);
        return *this;
      }
    }

    bool isEmpty () const { return this->indices.empty () && this->hasChildren () == false; }

    void deleteElement (unsigned int index)
    {
      unsigned int n = this->indices.erase (index);
      assert (n > 0);
      unused (n);
    }

    bool deleteEmptyChildren ()
    {
      bool allChildrenEmpty = true;

      for (unsigned int i = 0; i < 8; i++)
      {
        if (this->children[i])
        {
          if (this->children[i]->deleteEmptyChildren ())
          {
            this->children[i].reset ();
          }
          else
          {
            allChildrenEmpty = false;
          }
        }
      }

      if (allChildrenEmpty)
      {
        assert (this->hasChildren () == false);
        return this->indices.empty ();
      }
      else
      {
        return false;
      }
    }

#ifdef DILAY_RENDER_OCTREE
    void render (Camera& camera, Mesh& nodeMesh) const
    {
      nodeMesh.position (this->center);
      nodeMesh.scaling (glm::vec3 (this->width * 0.5f));
      nodeMesh.renderLines (camera);

      for (unsigned int i = 0; i < 8; i++)
      {
        if (this->children[i])
        {
          this->children[i]->render (camera, nodeMesh);
        }
      }
    }
#endif

    template <typename T>
    void containsOrIntersectsT (const T&                                           t,
                                const DynamicOctree::ContainsIntersectionCallback& f) const
    {
      const bool contains = t.contains (this->looseAABox);

      if (contains || IntersectionUtil::intersects (t, this->looseAABox))
      {
        for (unsigned int index : this->indices)
        {
          f (contains, index);
        }
        for (unsigned int i = 0; i < 8; i++)
        {
          if (this->children[i])
          {
            this->children[i]->containsOrIntersectsT<T> (t, f);
          }
        }
      }
    }

    template <typename T>
    void intersectsT (const T& t, const DynamicOctree::IntersectionCallback& f) const
    {
      if (IntersectionUtil::intersects (t, this->looseAABox))
      {
        for (unsigned int index : this->indices)
        {
          f (index);
        }
        for (unsigned int i = 0; i < 8; i++)
        {
          if (this->children[i])
          {
            this->children[i]->intersectsT<T> (t, f);
          }
        }
      }
    }

    void intersects (const PrimRay& ray, float& distance,
                     const DynamicOctree::RayIntersectionCallback& f) const
    {
      float t;
      if (IntersectionUtil::intersects (ray, this->looseAABox, &t) && t < distance)
      {
        for (unsigned int index : this->indices)
        {
          distance = glm::min (f (index), distance);
        }
        for (unsigned int i = 0; i < 8; i++)
        {
          if (this->children[i])
          {
            this->children[i]->intersects (ray, distance, f);
          }
        }
      }
    }

    void distance (PrimSphere& sphere, const DynamicOctree::DistanceCallback& getDistance) const
    {
      for (unsigned int i : this->indices)
      {
        const float distance = getDistance (i);
        if (distance < sphere.radius ())
        {
          sphere.radius (distance);
        }
      }

      const unsigned int first = this->childIndex (sphere.center ());
      const bool         hasFirst = bool(this->children[first]);
      if (hasFirst && IntersectionUtil::intersects (sphere, this->children[first]->looseAABox))
      {
        this->children[first]->distance (sphere, getDistance);
      }

      for (unsigned int i = 0; i < 8; i++)
      {
        const bool hasChild = i != first && bool(this->children[i]);
        if (hasChild && IntersectionUtil::intersects (sphere, this->children[i]->looseAABox))
        {
          this->children[i]->distance (sphere, getDistance);
        }
      }
    }

    unsigned int numElements () const { return this->indices.size (); }

    void updateIndices (const std::vector<unsigned int>& indexMap)
    {
      std::unordered_set<unsigned int> newIndices;

      for (unsigned int i : this->indices)
      {
        assert (indexMap[i] != Util::invalidIndex ());

        newIndices.insert (indexMap[i]);
      }
      this->indices = std::move (newIndices);

      for (unsigned int i = 0; i < 8; i++)
      {
        if (this->children[i])
        {
          this->children[i]->updateIndices (indexMap);
        }
      }
    }

    void updateStatistics (IndexOctreeStatistics& stats) const
    {
      stats.numNodes += 1;
      stats.numElements += this->numElements ();
      stats.minDepth = glm::min (stats.minDepth, this->depth);
      stats.maxDepth = glm::max (stats.maxDepth, this->depth);
      stats.maxElementsPerNode = glm::max (stats.maxElementsPerNode, this->numElements ());

      auto e = stats.numElementsPerDepth.find (this->depth);
      if (e == stats.numElementsPerDepth.end ())
      {
        stats.numElementsPerDepth.emplace (this->depth, this->numElements ());
      }
      else
      {
        e->second = e->second + this->numElements ();
      }
      e = stats.numNodesPerDepth.find (this->depth);
      if (e == stats.numNodesPerDepth.end ())
      {
        stats.numNodesPerDepth.emplace (this->depth, 1);
      }
      else
      {
        e->second = e->second + 1;
      }
      for (unsigned int i = 0; i < 8; i++)
      {
        if (this->children[i])
        {
          this->children[i]->updateStatistics (stats);
        }
      }
    }
  };
}

struct DynamicOctree::Impl
{
  Child                         root;
  std::vector<IndexOctreeNode*> elementNodeMap;

  Impl () {}

  Impl (const Impl& other)
    : root (other.root)
  {
    this->makeElementNodeMap ();
  }

  bool hasRoot () const { return bool(this->root); }

  void setupRoot (const glm::vec3& position, float width)
  {
    assert (this->hasRoot () == false);
    this->root = IndexOctreeNode (position, width, 0);
  }

  void makeElementNodeMap ()
  {
    std::function<void(IndexOctreeNode&)> traverse = [this, &traverse](IndexOctreeNode& node) {
      for (unsigned i : node.indices)
      {
        this->addToElementNodeMap (i, node);
      }
      for (unsigned int i = 0; i < 8; i++)
      {
        if (node.children[i])
        {
          traverse (*node.children[i]);
        }
      }
    };
    if (this->root)
    {
      traverse (*this->root);
    }
  }

  void addToElementNodeMap (unsigned int index, IndexOctreeNode& node)
  {
    if (index >= this->elementNodeMap.size ())
    {
      this->elementNodeMap.resize (index + 1, nullptr);
    }
    assert (this->elementNodeMap[index] == nullptr);
    this->elementNodeMap[index] = &node;
  }

  void makeParent (const glm::vec3& position)
  {
    assert (this->hasRoot ());

    const glm::vec3 rootCenter = this->root->center;
    const float     rootWidth = this->root->width;
    const float     halfRootWidth = this->root->width * 0.5f;
    glm::vec3       parentCenter;
    int             index = 0;

    if (rootCenter.x < position.x)
      parentCenter.x = rootCenter.x + halfRootWidth;
    else
    {
      parentCenter.x = rootCenter.x - halfRootWidth;
      index += 4;
    }
    if (rootCenter.y < position.y)
      parentCenter.y = rootCenter.y + halfRootWidth;
    else
    {
      parentCenter.y = rootCenter.y - halfRootWidth;
      index += 2;
    }
    if (rootCenter.z < position.z)
      parentCenter.z = rootCenter.z + halfRootWidth;
    else
    {
      parentCenter.z = rootCenter.z - halfRootWidth;
      index += 1;
    }

    IndexOctreeNode* newRoot =
      new IndexOctreeNode (parentCenter, rootWidth * 2.0f, this->root->depth - 1);
    newRoot->children[index] = std::move (this->root);
    this->root.reset (newRoot);
  }

  void addElement (unsigned int index, const glm::vec3& position, float maxDimExtent)
  {
    assert (this->hasRoot ());

    if (this->root->approxContains (position, maxDimExtent))
    {
      IndexOctreeNode& node = this->root->addElement (index, position, maxDimExtent);
      this->addToElementNodeMap (index, node);
    }
    else
    {
      this->makeParent (position);
      this->addElement (index, position, maxDimExtent);
    }
  }

  void realignElement (unsigned int index, const glm::vec3& position, float maxDimExtent)
  {
    assert (this->hasRoot ());
    assert (index < this->elementNodeMap.size ());
    assert (this->elementNodeMap[index]);

    IndexOctreeNode* node = this->elementNodeMap[index];

    if (node->approxContains (position, maxDimExtent) == false ||
        node->insertIntoChild (maxDimExtent))
    {
      this->deleteElement (index);
      this->addElement (index, position, maxDimExtent);
    }
  }

  void deleteElement (unsigned int index)
  {
    assert (index < this->elementNodeMap.size ());
    assert (this->elementNodeMap[index]);

    this->elementNodeMap[index]->deleteElement (index);
    this->elementNodeMap[index] = nullptr;

    if (this->hasRoot ())
    {
      if (this->root->isEmpty ())
      {
        this->root.reset ();
      }
      else
      {
        this->shrinkRoot ();
      }
    }
  }

  void deleteEmptyChildren ()
  {
    if (this->hasRoot ())
    {
      if (this->root->deleteEmptyChildren ())
      {
        this->root.reset ();
      }
    }
  }

  void updateIndices (const std::vector<unsigned int>& newIndices)
  {
    for (unsigned int i = 0; i < newIndices.size (); i++)
    {
      const unsigned int newI = newIndices[i];
      if (newI != Util::invalidIndex () && newI != i)
      {
        assert (i < this->elementNodeMap.size ());
        assert (newI < this->elementNodeMap.size ());
        assert (this->elementNodeMap[i]);
        assert (this->elementNodeMap[newI] == nullptr);

        this->elementNodeMap[newI] = this->elementNodeMap[i];
        this->elementNodeMap[i] = nullptr;
      }
    }
    this->elementNodeMap.resize (newIndices.size ());

    if (this->hasRoot ())
    {
      this->root->updateIndices (newIndices);
    }
  }

  void shrinkRoot ()
  {
    if (this->hasRoot () && this->root->indices.empty () && this->root->hasChildren ())
    {
      int singleNonEmptyChildIndex = -1;
      for (int i = 0; i < 8; i++)
      {
        if (this->root->children[i] && this->root->children[i]->isEmpty () == false)
        {
          if (singleNonEmptyChildIndex == -1)
          {
            singleNonEmptyChildIndex = i;
          }
          else
          {
            return;
          }
        }
      }
      if (singleNonEmptyChildIndex != -1)
      {
        this->root = std::move (this->root->children[singleNonEmptyChildIndex]);
        this->shrinkRoot ();
      }
    }
  }

  void reset ()
  {
    this->root.reset ();
    this->elementNodeMap.clear ();
  }

#ifdef DILAY_RENDER_OCTREE
  void render (Camera& camera) const
  {
    Mesh nodeMesh;
    nodeMesh.addVertex (glm::vec3 (-1.0f, -1.0f, -1.0f));
    nodeMesh.addVertex (glm::vec3 (-1.0f, -1.0f, 1.0f));
    nodeMesh.addVertex (glm::vec3 (-1.0f, 1.0f, -1.0f));
    nodeMesh.addVertex (glm::vec3 (-1.0f, 1.0f, 1.0f));
    nodeMesh.addVertex (glm::vec3 (1.0f, -1.0f, -1.0f));
    nodeMesh.addVertex (glm::vec3 (1.0f, -1.0f, 1.0f));
    nodeMesh.addVertex (glm::vec3 (1.0f, 1.0f, -1.0f));
    nodeMesh.addVertex (glm::vec3 (1.0f, 1.0f, 1.0f));

    nodeMesh.addIndex (0);
    nodeMesh.addIndex (1);
    nodeMesh.addIndex (1);
    nodeMesh.addIndex (3);
    nodeMesh.addIndex (3);
    nodeMesh.addIndex (2);
    nodeMesh.addIndex (2);
    nodeMesh.addIndex (0);

    nodeMesh.addIndex (4);
    nodeMesh.addIndex (5);
    nodeMesh.addIndex (5);
    nodeMesh.addIndex (7);
    nodeMesh.addIndex (7);
    nodeMesh.addIndex (6);
    nodeMesh.addIndex (6);
    nodeMesh.addIndex (4);

    nodeMesh.addIndex (1);
    nodeMesh.addIndex (5);
    nodeMesh.addIndex (5);
    nodeMesh.addIndex (7);
    nodeMesh.addIndex (7);
    nodeMesh.addIndex (3);
    nodeMesh.addIndex (3);
    nodeMesh.addIndex (1);

    nodeMesh.addIndex (4);
    nodeMesh.addIndex (6);
    nodeMesh.addIndex (6);
    nodeMesh.addIndex (2);
    nodeMesh.addIndex (2);
    nodeMesh.addIndex (0);
    nodeMesh.addIndex (0);
    nodeMesh.addIndex (4);

    nodeMesh.renderMode ().constantShading (true);
    nodeMesh.renderMode ().noDepthTest (true);
    nodeMesh.color (Color (1.0f, 1.0f, 0.0f));
    nodeMesh.bufferData ();

    if (this->hasRoot ())
    {
      this->root->render (camera, nodeMesh);
    }
  }
#else
  void render (Camera&) const { DILAY_IMPOSSIBLE }
#endif

  void intersects (const PrimRay& ray, const DynamicOctree::RayIntersectionCallback& f) const
  {
    if (this->hasRoot ())
    {
      float distance = Util::maxFloat ();
      return this->root->intersects (ray, distance, f);
    }
  }

  void intersects (const PrimPlane& plane, const DynamicOctree::IntersectionCallback& f) const
  {
    if (this->hasRoot ())
    {
      return this->root->intersectsT<PrimPlane> (plane, f);
    }
  }

  void intersects (const PrimSphere&                                  sphere,
                   const DynamicOctree::ContainsIntersectionCallback& f) const
  {
    if (this->hasRoot ())
    {
      return this->root->containsOrIntersectsT<PrimSphere> (sphere, f);
    }
  }

  void intersects (const PrimAABox& box, const DynamicOctree::ContainsIntersectionCallback& f) const
  {
    if (this->hasRoot ())
    {
      return this->root->containsOrIntersectsT<PrimAABox> (box, f);
    }
  }

  float distance (const glm::vec3& p, const DistanceCallback& getDistance) const
  {
    assert (this->hasRoot ());
    PrimSphere sphere (p, Util::maxFloat ());
    this->root->distance (sphere, getDistance);
    return sphere.radius ();
  }

  void printStatistics () const
  {
    IndexOctreeStatistics stats{0,
                                0,
                                Util::maxInt (),
                                Util::minInt (),
                                0,
                                IndexOctreeStatistics::DepthMap (),
                                IndexOctreeStatistics::DepthMap ()};
    if (this->hasRoot ())
    {
      this->root->updateStatistics (stats);
    }
    std::cout << "octree:"
              << "\n\tnum nodes:\t\t\t" << stats.numNodes << "\n\tnum elements:\t\t\t"
              << stats.numElements << "\n\tmax elements per node:\t\t" << stats.maxElementsPerNode
              << "\n\tmin depth:\t\t\t" << stats.minDepth << "\n\tmax depth:\t\t\t"
              << stats.maxDepth << "\n\telements per node:\t\t"
              << float(stats.numElements) / float(stats.numNodes) << std::endl;
  }
};

DELEGATE_BIG4_COPY (DynamicOctree)

DELEGATE_CONST (bool, DynamicOctree, hasRoot)
DELEGATE2 (void, DynamicOctree, setupRoot, const glm::vec3&, float)
DELEGATE3 (void, DynamicOctree, addElement, unsigned int, const glm::vec3&, float)
DELEGATE3 (void, DynamicOctree, realignElement, unsigned int, const glm::vec3&, float)
DELEGATE1 (void, DynamicOctree, deleteElement, unsigned int)
DELEGATE (void, DynamicOctree, deleteEmptyChildren)
DELEGATE1 (void, DynamicOctree, updateIndices, const std::vector<unsigned int>&)
DELEGATE (void, DynamicOctree, shrinkRoot)
DELEGATE (void, DynamicOctree, reset)
DELEGATE1_CONST (void, DynamicOctree, render, Camera&)
DELEGATE2_CONST (void, DynamicOctree, intersects, const PrimRay&,
                 const DynamicOctree::RayIntersectionCallback&)
DELEGATE2_CONST (void, DynamicOctree, intersects, const PrimPlane&,
                 const DynamicOctree::IntersectionCallback&)
DELEGATE2_CONST (void, DynamicOctree, intersects, const PrimSphere&,
                 const DynamicOctree::ContainsIntersectionCallback&)
DELEGATE2_CONST (void, DynamicOctree, intersects, const PrimAABox&,
                 const DynamicOctree::ContainsIntersectionCallback&)
DELEGATE2_CONST (float, DynamicOctree, distance, const glm::vec3&,
                 const DynamicOctree::DistanceCallback&)
DELEGATE_CONST (void, DynamicOctree, printStatistics)
