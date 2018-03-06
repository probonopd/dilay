/* This file is part of Dilay
 * Copyright © 2015-2018 Alexander Bau
 * Use and redistribute under the terms of the GNU General Public License
 */
#include <algorithm>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <unordered_map>
#include <vector>
#include "hash.hpp"
#include "intersection.hpp"
#include "mesh-util.hpp"
#include "mesh.hpp"
#include "primitive/plane.hpp"
#include "primitive/ray.hpp"
#include "util.hpp"

namespace
{
  struct VertexCache
  {
    typedef std::function<unsigned int(unsigned int, unsigned int)> MakeNewVertex;

    std::unordered_map<ui_pair, unsigned int> cache;

    unsigned int lookup (unsigned int i1, unsigned int i2, const MakeNewVertex& f)
    {
      const ui_pair key = std::make_pair (glm::min (i1, i2), glm::max (i1, i2));

      const auto it = this->cache.find (key);
      if (it == this->cache.end ())
      {
        const unsigned int n = f (i1, i2);
        this->cache.emplace (key, n);
        return n;
      }
      else
      {
        return it->second;
      }
    }
  };

  struct EdgeMap
  {
    EdgeMap (unsigned int numVertices) { this->elements.resize (numVertices - 1); }

    unsigned int* find (unsigned int i1, unsigned int i2)
    {
      const unsigned int minI = glm::min (i1, i2);
      const unsigned int maxI = glm::max (i1, i2);

      if (minI < this->elements.size ())
      {
        return this->findInSequence (elements[minI], maxI);
      }
      else
      {
        return nullptr;
      }
    }

    void add (unsigned int i1, unsigned int i2, const unsigned int& element)
    {
      const unsigned int minI = glm::min (i1, i2);
      const unsigned int maxI = glm::max (i1, i2);

      assert (minI < this->elements.size ());
      assert (this->findInSequence (this->elements[minI], maxI) == nullptr);

      if (this->elements[minI].empty ())
      {
        this->elements[minI].reserve (6);
      }
      this->elements[minI].push_back (std::make_pair (maxI, element));
    }

    void increase (unsigned int i1, unsigned int i2)
    {
      const unsigned int minI = glm::min (i1, i2);
      const unsigned int maxI = glm::max (i1, i2);

      assert (minI < this->elements.size ());

      unsigned int* value = this->findInSequence (this->elements[minI], maxI);
      if (value)
      {
        (*value)++;
      }
      else
      {
        if (this->elements[minI].empty ())
        {
          this->elements[minI].reserve (6);
        }
        this->elements[minI].push_back (std::make_pair (maxI, 1));
      }
    }

    unsigned int* findInSequence (std::vector<std::pair<unsigned int, unsigned int>>& sequence,
                                  unsigned int                                        i)
    {
      for (auto& p : sequence)
      {
        if (p.first == i)
        {
          return &p.second;
        }
      }
      return nullptr;
    }

    std::vector<std::vector<std::pair<unsigned int, unsigned int>>> elements;
  };

  Mesh& withDefaultNormals (Mesh& mesh)
  {
    for (unsigned int i = 0; i < mesh.numVertices (); i++)
    {
      mesh.normal (i, glm::normalize (mesh.vertex (i)));
    }
    return mesh;
  }
}

void MeshUtil::addFace (Mesh& mesh, unsigned int i1, unsigned int i2, unsigned int i3)
{
  mesh.addIndex (i1);
  mesh.addIndex (i2);
  mesh.addIndex (i3);
}

void MeshUtil::addFace (Mesh& mesh, unsigned int i1, unsigned int i2, unsigned int i3,
                        unsigned int i4)
{
  mesh.addIndex (i1);
  mesh.addIndex (i2);
  mesh.addIndex (i3);
  mesh.addIndex (i4);
  mesh.addIndex (i1);
  mesh.addIndex (i3);
}

Mesh MeshUtil::cube (unsigned int numSubdivisions)
{
  Mesh        mesh;
  VertexCache vertexCache;

  const auto addStartVertex = [&mesh](const glm::vec3& v) -> unsigned int {
    return mesh.addVertex (v, glm::normalize (v));
  };

  const auto addRefinedVertex = [&mesh](unsigned int i1, unsigned int i2) -> unsigned int {
    return mesh.addVertex (Util::midpoint (mesh.vertex (i1), mesh.vertex (i2)));
  };

  std::function<void(unsigned int, unsigned int, unsigned int, unsigned int, unsigned int)>
    subdivide = [&mesh, &subdivide, &addRefinedVertex,
                 &vertexCache](unsigned int s, unsigned int i1, unsigned int i2, unsigned int i3,
                               unsigned int i4) -> void {
    if (s == 0)
    {
      MeshUtil::addFace (mesh, i1, i2, i3);
      MeshUtil::addFace (mesh, i1, i3, i4);
    }
    else
    {
      const glm::vec3    v1 = mesh.vertex (i1);
      const glm::vec3    v2 = mesh.vertex (i2);
      const glm::vec3    v3 = mesh.vertex (i3);
      const glm::vec3    v4 = mesh.vertex (i4);
      const unsigned int iC = mesh.addVertex ((v1 + v2 + v3 + v4) / 4.0f);
      const unsigned int i12 = vertexCache.lookup (i1, i2, addRefinedVertex);
      const unsigned int i23 = vertexCache.lookup (i2, i3, addRefinedVertex);
      const unsigned int i34 = vertexCache.lookup (i3, i4, addRefinedVertex);
      const unsigned int i41 = vertexCache.lookup (i4, i1, addRefinedVertex);

      subdivide (s - 1, i1, i12, iC, i41);
      subdivide (s - 1, i2, i23, iC, i12);
      subdivide (s - 1, i3, i34, iC, i23);
      subdivide (s - 1, i4, i41, iC, i34);
    }
  };

  addStartVertex (glm::vec3 (-0.5f, -0.5f, -0.5f));
  addStartVertex (glm::vec3 (-0.5f, -0.5f, +0.5f));
  addStartVertex (glm::vec3 (-0.5f, +0.5f, -0.5f));
  addStartVertex (glm::vec3 (-0.5f, +0.5f, +0.5f));
  addStartVertex (glm::vec3 (+0.5f, -0.5f, -0.5f));
  addStartVertex (glm::vec3 (+0.5f, -0.5f, +0.5f));
  addStartVertex (glm::vec3 (+0.5f, +0.5f, -0.5f));
  addStartVertex (glm::vec3 (+0.5f, +0.5f, +0.5f));

  subdivide (numSubdivisions, 0, 1, 3, 2);
  subdivide (numSubdivisions, 1, 5, 7, 3);
  subdivide (numSubdivisions, 5, 4, 6, 7);
  subdivide (numSubdivisions, 4, 0, 2, 6);
  subdivide (numSubdivisions, 3, 7, 6, 2);
  subdivide (numSubdivisions, 0, 4, 5, 1);

  for (unsigned int i = 0; i < mesh.numVertices (); i++)
  {
    const glm::vec3& vertex = mesh.vertex (i);
    glm::vec3        normal = glm::vec3 (0.0f);

    if (Util::almostEqual (vertex.x, 0.5f))
    {
      normal.x = 1.0f;
    }
    else if (Util::almostEqual (vertex.x, -0.5f))
    {
      normal.x = -1.0f;
    }

    if (Util::almostEqual (vertex.y, 0.5f))
    {
      normal.y = 1.0f;
    }
    else if (Util::almostEqual (vertex.y, -0.5f))
    {
      normal.y = -1.0f;
    }

    if (Util::almostEqual (vertex.z, 0.5f))
    {
      normal.z = 1.0f;
    }
    else if (Util::almostEqual (vertex.z, -0.5f))
    {
      normal.z = -1.0f;
    }
    mesh.normal (i, glm::normalize (normal));
  }
  return mesh;
}

Mesh MeshUtil::sphere (unsigned int rings, unsigned int sectors)
{
  assert (rings > 1 && sectors > 2);
  Mesh mesh;

  const float radius = 1.0f;
  const float ringStep = glm::pi<float> () / float(rings);
  const float sectorStep = 2.0f * glm::pi<float> () / float(sectors);
  float       phi = ringStep;
  float       theta = 0.0f;

  // inner rings vertices
  for (unsigned int r = 0; r < rings - 1; r++)
  {
    for (unsigned int s = 0; s < sectors; s++)
    {
      const float x = radius * sin (theta) * sin (phi);
      const float y = radius * cos (phi);
      const float z = radius * cos (theta) * sin (phi);

      mesh.addVertex (glm::vec3 (x, y, z));

      theta += sectorStep;
    }
    phi += ringStep;
  }

  // caps vertices
  const unsigned int topCapIndex = mesh.addVertex (glm::vec3 (0.0f, radius, 0.0f));
  const unsigned int botCapIndex = mesh.addVertex (glm::vec3 (0.0f, -radius, 0.0f));

  // inner rings indices
  for (unsigned int r = 0; r < rings - 2; r++)
  {
    for (unsigned int s = 0; s < sectors; s++)
    {
      MeshUtil::addFace (mesh, (sectors * r) + s, (sectors * (r + 1)) + s,
                         (sectors * (r + 1)) + ((s + 1) % sectors),
                         (sectors * r) + ((s + 1) % sectors));
    }
  }

  // caps indices
  for (unsigned int s = 0; s < sectors; s++)
  {
    MeshUtil::addFace (mesh, topCapIndex, s, (s + 1) % sectors);

    MeshUtil::addFace (mesh, botCapIndex, (sectors * (rings - 2)) + ((s + 1) % sectors),
                       (sectors * (rings - 2)) + s);
  }
  return withDefaultNormals (mesh);
}

Mesh MeshUtil::icosphere (unsigned int numSubdivisions)
{
  Mesh        mesh;
  VertexCache vertexCache;

  const auto addStartVertex = [&mesh](const glm::vec3& v) -> unsigned int {
    return mesh.addVertex (glm::normalize (v), glm::normalize (v));
  };

  const auto addRefinedVertex = [&mesh](unsigned int i1, unsigned int i2) -> unsigned int {
    const glm::vec3 pos = glm::normalize (Util::midpoint (mesh.vertex (i1), mesh.vertex (i2)));
    const glm::vec3 normal = glm::normalize (pos);
    return mesh.addVertex (pos, normal);
  };

  std::function<void(unsigned int, unsigned int, unsigned int, unsigned int)> subdivide =
    [&mesh, &subdivide, &addRefinedVertex, &vertexCache](unsigned int s, unsigned int i1,
                                                         unsigned int i2, unsigned int i3) -> void {
    if (s == 0)
    {
      MeshUtil::addFace (mesh, i1, i2, i3);
    }
    else
    {
      const unsigned int i12 = vertexCache.lookup (i1, i2, addRefinedVertex);
      const unsigned int i23 = vertexCache.lookup (i2, i3, addRefinedVertex);
      const unsigned int i31 = vertexCache.lookup (i3, i1, addRefinedVertex);

      subdivide (s - 1, i1, i12, i31);
      subdivide (s - 1, i2, i23, i12);
      subdivide (s - 1, i3, i31, i23);
      subdivide (s - 1, i12, i23, i31);
    }
  };

  const float t = (1.0f + glm::sqrt (5.0f)) * 0.5f;

  addStartVertex (glm::vec3 (-1.0f, +t, 0.0f));
  addStartVertex (glm::vec3 (+1.0f, +t, 0.0f));
  addStartVertex (glm::vec3 (-1.0f, -t, 0.0f));
  addStartVertex (glm::vec3 (+1.0f, -t, 0.0f));

  addStartVertex (glm::vec3 (0.0f, -1.0f, +t));
  addStartVertex (glm::vec3 (0.0f, +1.0f, +t));
  addStartVertex (glm::vec3 (0.0f, -1.0f, -t));
  addStartVertex (glm::vec3 (0.0f, +1.0f, -t));

  addStartVertex (glm::vec3 (+t, 0.0f, -1.0f));
  addStartVertex (glm::vec3 (+t, 0.0f, +1.0f));
  addStartVertex (glm::vec3 (-t, 0.0f, -1.0f));
  addStartVertex (glm::vec3 (-t, 0.0f, +1.0f));

  subdivide (numSubdivisions, 0, 11, 5);
  subdivide (numSubdivisions, 0, 5, 1);
  subdivide (numSubdivisions, 0, 1, 7);
  subdivide (numSubdivisions, 0, 7, 10);
  subdivide (numSubdivisions, 0, 10, 11);

  subdivide (numSubdivisions, 1, 5, 9);
  subdivide (numSubdivisions, 5, 11, 4);
  subdivide (numSubdivisions, 11, 10, 2);
  subdivide (numSubdivisions, 10, 7, 6);
  subdivide (numSubdivisions, 7, 1, 8);

  subdivide (numSubdivisions, 3, 9, 4);
  subdivide (numSubdivisions, 3, 4, 2);
  subdivide (numSubdivisions, 3, 2, 6);
  subdivide (numSubdivisions, 3, 6, 8);
  subdivide (numSubdivisions, 3, 8, 9);

  subdivide (numSubdivisions, 4, 9, 5);
  subdivide (numSubdivisions, 2, 4, 11);
  subdivide (numSubdivisions, 6, 2, 10);
  subdivide (numSubdivisions, 8, 6, 7);
  subdivide (numSubdivisions, 9, 8, 1);

  return mesh;
}

Mesh MeshUtil::cone (unsigned int numBaseVertices)
{
  assert (numBaseVertices >= 3);

  Mesh        mesh;
  const float c = 2.0f * glm::pi<float> () / float(numBaseVertices);

  for (unsigned int i = 0; i < numBaseVertices; i++)
  {
    mesh.addVertex (glm::vec3 (glm::sin (float(i) * c), -0.5f, glm::cos (float(i) * c)));
  }
  mesh.addVertex (glm::vec3 (0.0f, -0.5f, 0.0f));
  mesh.addVertex (glm::vec3 (0.0f, 0.5f, 0.0f));

  for (unsigned int i = 0; i < numBaseVertices - 1; i++)
  {
    MeshUtil::addFace (mesh, i, i + 1, numBaseVertices + 1);
    MeshUtil::addFace (mesh, i + 1, i, numBaseVertices);
  }
  MeshUtil::addFace (mesh, numBaseVertices - 1, 0, numBaseVertices + 1);
  MeshUtil::addFace (mesh, 0, numBaseVertices - 1, numBaseVertices);

  return withDefaultNormals (mesh);
}

Mesh MeshUtil::cylinder (unsigned int numVertices)
{
  assert (numVertices >= 3);

  Mesh        mesh;
  const float c = 2.0f * glm::pi<float> () / float(numVertices);

  for (unsigned int i = 0; i < numVertices; i++)
  {
    mesh.addVertex (glm::vec3 (glm::sin (float(i) * c), -0.5f, glm::cos (float(i) * c)));
  }
  for (unsigned int i = 0; i < numVertices; i++)
  {
    mesh.addVertex (glm::vec3 (glm::sin (float(i) * c), 0.5f, glm::cos (float(i) * c)));
  }
  mesh.addVertex (glm::vec3 (0.0f, -0.5f, 0.0f));
  mesh.addVertex (glm::vec3 (0.0f, 0.5f, 0.0f));

  for (unsigned int i = 0; i < numVertices - 1; i++)
  {
    MeshUtil::addFace (mesh, i, i + 1, i + numVertices + 1, i + numVertices);

    MeshUtil::addFace (mesh, i + 1, i, 2 * numVertices);
    MeshUtil::addFace (mesh, i + numVertices, i + numVertices + 1, (2 * numVertices) + 1);
  }
  MeshUtil::addFace (mesh, numVertices - 1, 0, numVertices, (2 * numVertices) - 1);

  MeshUtil::addFace (mesh, 0, numVertices - 1, 2 * numVertices);
  MeshUtil::addFace (mesh, (2 * numVertices) - 1, numVertices, (2 * numVertices) + 1);

  return withDefaultNormals (mesh);
}

Mesh MeshUtil::mirror (const Mesh& mesh, const PrimPlane& plane)
{
  assert (MeshUtil::checkConsistency (mesh));

  enum class Side
  {
    Negative,
    Border,
    Positive
  };
  enum class BorderFlag
  {
    NoBorder,
    ConnectsNegative,
    ConnectsPositive,
    ConnectsBoth
  };

  auto side = [&plane](const glm::vec3& v) -> Side {
    const float eps = Util::epsilon () * 0.5f;
    const float d = plane.distance (v);

    return d < -eps ? Side::Negative : (d > eps ? Side::Positive : Side::Border);
  };

  Mesh                    m;
  std::vector<Side>       sides;
  std::vector<BorderFlag> borderFlags;
  std::vector<ui_pair>    newIndices;
  EdgeMap                 newBorderVertices (mesh.numVertices ());

  auto updateBorderFlag = [&borderFlags](unsigned int i, Side side) {
    BorderFlag& current = borderFlags[i];

    if (side == Side::Border)
    {
    }
    else if (side == Side::Negative)
    {
      if (current == BorderFlag::NoBorder)
      {
        current = BorderFlag::ConnectsNegative;
      }
      else if (current == BorderFlag::ConnectsPositive)
      {
        current = BorderFlag::ConnectsBoth;
      }
    }
    else if (side == Side::Positive)
    {
      if (current == BorderFlag::NoBorder)
      {
        current = BorderFlag::ConnectsPositive;
      }
      else if (current == BorderFlag::ConnectsNegative)
      {
        current = BorderFlag::ConnectsBoth;
      }
    }
  };

  auto newBorderVertex = [&mesh, &plane, &newBorderVertices, &m](unsigned int i1,
                                                                 unsigned int i2) -> unsigned int {
    const unsigned int* existentIndex = newBorderVertices.find (i1, i2);

    if (existentIndex)
    {
      return *existentIndex;
    }
    else
    {
      const glm::vec3 v1 (mesh.vertex (i1));
      const glm::vec3 v2 (mesh.vertex (i2));
      const PrimRay   ray (true, v1, v2 - v1);

      float     t;
      glm::vec3 position;

      if (IntersectionUtil::intersects (ray, plane, &t))
      {
        position = ray.pointAt (t);
      }
      else
      {
        position = (v1 + v2) * 0.5f;
      }
      const unsigned int newIndex = m.addVertex (position);

      newBorderVertices.add (i1, i2, newIndex);
      return newIndex;
    }
  };

  m.copyNonGeometry (mesh);

  sides.reserve (mesh.numVertices ());
  borderFlags.reserve (mesh.numVertices ());

  newIndices.resize (mesh.numVertices (),
                     std::make_pair (Util::invalidIndex (), Util::invalidIndex ()));

  // cache data
  for (unsigned int i = 0; i < mesh.numVertices (); i++)
  {
    sides.push_back (side (mesh.vertex (i)));
    borderFlags.push_back (BorderFlag::NoBorder);
  }

  // update border flags
  for (unsigned int i = 0; i < mesh.numIndices (); i += 3)
  {
    const unsigned int i1 = mesh.index (i + 0);
    const unsigned int i2 = mesh.index (i + 1);
    const unsigned int i3 = mesh.index (i + 2);

    assert (sides[i1] != Side::Border || sides[i2] != Side::Border || sides[i3] != Side::Border);

    updateBorderFlag (i1, sides[i2]);
    updateBorderFlag (i1, sides[i3]);
    updateBorderFlag (i2, sides[i1]);
    updateBorderFlag (i2, sides[i3]);
    updateBorderFlag (i3, sides[i1]);
    updateBorderFlag (i3, sides[i2]);
  }

  // mirror vertices
  for (unsigned int i = 0; i < mesh.numVertices (); i++)
  {
    switch (sides[i])
    {
      case Side::Negative:
        break;

      case Side::Border:
      {
        switch (borderFlags[i])
        {
          case BorderFlag::NoBorder:
            DILAY_IMPOSSIBLE
            break;
          case BorderFlag::ConnectsNegative:
            break;
          case BorderFlag::ConnectsPositive:
          {
            const unsigned int index1 = m.addVertex (mesh.vertex (i));
            const unsigned int index2 = m.addVertex (mesh.vertex (i));

            newIndices[i] = std::make_pair (index1, index2);
            break;
          }
          case BorderFlag::ConnectsBoth:
          {
            const unsigned int index = m.addVertex (mesh.vertex (i));

            newIndices[i] = std::make_pair (index, index);
            break;
          }
        }
        break;
      }
      case Side::Positive:
      {
        const unsigned int index1 = m.addVertex (mesh.vertex (i));
        const unsigned int index2 = m.addVertex (plane.mirror (mesh.vertex (i)));

        newIndices[i] = std::make_pair (index1, index2);
        break;
      }
    }
  }

  // mirror faces
  for (unsigned int i = 0; i < mesh.numIndices (); i += 3)
  {
    const unsigned int oldIndex1 = mesh.index (i + 0);
    const unsigned int oldIndex2 = mesh.index (i + 1);
    const unsigned int oldIndex3 = mesh.index (i + 2);

    const Side s1 = sides[oldIndex1];
    const Side s2 = sides[oldIndex2];
    const Side s3 = sides[oldIndex3];

    if (s1 == Side::Positive || s2 == Side::Positive || s3 == Side::Positive)
    {
      const ui_pair& new1 = newIndices[oldIndex1];
      const ui_pair& new2 = newIndices[oldIndex2];
      const ui_pair& new3 = newIndices[oldIndex3];

      // 3 non-negative
      if (s1 != Side::Negative && s2 != Side::Negative && s3 != Side::Negative)
      {
        MeshUtil::addFace (m, new1.first, new2.first, new3.first);
        MeshUtil::addFace (m, new3.second, new2.second, new1.second);
      }
      // 1 negative - 2 positive
      else if (s1 == Side::Positive && s2 == Side::Positive && s3 == Side::Negative)
      {
        unsigned int b1 = newBorderVertex (oldIndex1, oldIndex3);
        unsigned int b2 = newBorderVertex (oldIndex2, oldIndex3);

        MeshUtil::addFace (m, new2.first, b2, new1.first);
        MeshUtil::addFace (m, new1.second, b2, new2.second);

        MeshUtil::addFace (m, new1.first, b2, b1);
        MeshUtil::addFace (m, b1, b2, new1.second);
      }
      else if (s1 == Side::Positive && s2 == Side::Negative && s3 == Side::Positive)
      {
        unsigned int b1 = newBorderVertex (oldIndex1, oldIndex2);
        unsigned int b2 = newBorderVertex (oldIndex2, oldIndex3);

        MeshUtil::addFace (m, new1.first, b1, new3.first);
        MeshUtil::addFace (m, new3.second, b1, new1.second);

        MeshUtil::addFace (m, new3.first, b1, b2);
        MeshUtil::addFace (m, b2, b1, new3.second);
      }
      else if (s1 == Side::Negative && s2 == Side::Positive && s3 == Side::Positive)
      {
        unsigned int b1 = newBorderVertex (oldIndex1, oldIndex2);
        unsigned int b2 = newBorderVertex (oldIndex1, oldIndex3);

        MeshUtil::addFace (m, new3.first, b2, new2.first);
        MeshUtil::addFace (m, new2.second, b2, new3.second);

        MeshUtil::addFace (m, new2.first, b2, b1);
        MeshUtil::addFace (m, b1, b2, new2.second);
      }
      // 1 positive - 2 negative
      else if (s1 == Side::Positive && s2 == Side::Negative && s3 == Side::Negative)
      {
        unsigned int b1 = newBorderVertex (oldIndex1, oldIndex2);
        unsigned int b2 = newBorderVertex (oldIndex1, oldIndex3);

        MeshUtil::addFace (m, new1.first, b1, b2);
        MeshUtil::addFace (m, b2, b1, new1.second);
      }
      else if (s1 == Side::Negative && s2 == Side::Positive && s3 == Side::Negative)
      {
        unsigned int b1 = newBorderVertex (oldIndex1, oldIndex2);
        unsigned int b2 = newBorderVertex (oldIndex2, oldIndex3);

        MeshUtil::addFace (m, new2.first, b2, b1);
        MeshUtil::addFace (m, b1, b2, new2.second);
      }
      else if (s1 == Side::Negative && s2 == Side::Negative && s3 == Side::Positive)
      {
        unsigned int b1 = newBorderVertex (oldIndex1, oldIndex3);
        unsigned int b2 = newBorderVertex (oldIndex2, oldIndex3);

        MeshUtil::addFace (m, new3.first, b1, b2);
        MeshUtil::addFace (m, b2, b1, new3.second);
      }
      // 1 positive - 1 border - 1 negative
      else if (s1 == Side::Positive && s2 == Side::Border && s3 == Side::Negative)
      {
        assert (borderFlags[oldIndex2] == BorderFlag::ConnectsBoth);

        unsigned int b = newBorderVertex (oldIndex1, oldIndex3);

        MeshUtil::addFace (m, new1.first, new2.first, b);
        MeshUtil::addFace (m, b, new2.second, new1.second);
      }
      else if (s1 == Side::Border && s2 == Side::Positive && s3 == Side::Negative)
      {
        assert (borderFlags[oldIndex1] == BorderFlag::ConnectsBoth);

        unsigned int b = newBorderVertex (oldIndex2, oldIndex3);

        MeshUtil::addFace (m, new1.first, new2.first, b);
        MeshUtil::addFace (m, b, new2.second, new1.second);
      }
      else if (s1 == Side::Positive && s2 == Side::Negative && s3 == Side::Border)
      {
        assert (borderFlags[oldIndex3] == BorderFlag::ConnectsBoth);

        unsigned int b = newBorderVertex (oldIndex1, oldIndex2);

        MeshUtil::addFace (m, new1.first, b, new3.first);
        MeshUtil::addFace (m, new3.second, b, new1.second);
      }
      else if (s1 == Side::Border && s2 == Side::Negative && s3 == Side::Positive)
      {
        assert (borderFlags[oldIndex1] == BorderFlag::ConnectsBoth);

        unsigned int b = newBorderVertex (oldIndex2, oldIndex3);

        MeshUtil::addFace (m, new1.first, b, new3.first);
        MeshUtil::addFace (m, new3.second, b, new1.second);
      }
      else if (s1 == Side::Negative && s2 == Side::Positive && s3 == Side::Border)
      {
        assert (borderFlags[oldIndex3] == BorderFlag::ConnectsBoth);

        unsigned int b = newBorderVertex (oldIndex1, oldIndex2);

        MeshUtil::addFace (m, new2.first, new3.first, b);
        MeshUtil::addFace (m, b, new3.second, new2.second);
      }
      else if (s1 == Side::Negative && s2 == Side::Border && s3 == Side::Positive)
      {
        assert (borderFlags[oldIndex2] == BorderFlag::ConnectsBoth);

        unsigned int b = newBorderVertex (oldIndex1, oldIndex3);

        MeshUtil::addFace (m, new2.first, new3.first, b);
        MeshUtil::addFace (m, b, new3.second, new2.second);
      }
      else
      {
        DILAY_IMPOSSIBLE
      }
    }
  }

  if (MeshUtil::checkConsistency (m) == false)
  {
    m.reset ();
  }
  return m;
}

bool MeshUtil::checkConsistency (const Mesh& mesh)
{
  if (mesh.numVertices () == 0)
  {
    DILAY_WARN ("empty mesh");
    return false;
  }
  EdgeMap                   numEdgeAdjacentFaces (mesh.numVertices ());
  std::vector<unsigned int> numVertexAdjacentFaces;
  numVertexAdjacentFaces.resize (mesh.numVertices (), 0);

  for (unsigned int i = 0; i < mesh.numIndices (); i += 3)
  {
    const unsigned int i1 = mesh.index (i + 0);
    const unsigned int i2 = mesh.index (i + 1);
    const unsigned int i3 = mesh.index (i + 2);

    numVertexAdjacentFaces[i1]++;
    numVertexAdjacentFaces[i2]++;
    numVertexAdjacentFaces[i3]++;

    numEdgeAdjacentFaces.increase (i1, i2);
    numEdgeAdjacentFaces.increase (i1, i3);
    numEdgeAdjacentFaces.increase (i2, i3);
  }

  for (unsigned int v = 0; v < numVertexAdjacentFaces.size (); v++)
  {
    if (numVertexAdjacentFaces[v] < 3)
    {
      DILAY_WARN ("inconsistent vertex %u with %u adjacent faces", v, numVertexAdjacentFaces[v]);
      return false;
    }
  }

  for (unsigned int i = 0; i < numEdgeAdjacentFaces.elements.size (); i++)
  {
    for (const auto& pair : numEdgeAdjacentFaces.elements[i])
    {
      const unsigned int j = pair.first;
      const unsigned int value = pair.second;

      if (value != 2)
      {
        DILAY_WARN ("inconsistent edge (%u,%u) with %u adjacent faces", i, j, value);
        return false;
      }
    }
  }
  return true;
}
