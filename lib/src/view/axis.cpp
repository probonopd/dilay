/* This file is part of Dilay
 * Copyright © 2015-2018 Alexander Bau
 * Use and redistribute under the terms of the GNU General Public License
 */
#include <QPainter>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include "camera.hpp"
#include "color.hpp"
#include "config.hpp"
#include "dimension.hpp"
#include "mesh-util.hpp"
#include "mesh.hpp"
#include "opengl.hpp"
#include "render-mode.hpp"
#include "view/axis.hpp"

struct ViewAxis::Impl
{
  Mesh         coneMesh;
  Mesh         cylinderMesh;
  Mesh         gridMesh;
  glm::uvec2   axisResolution;
  Color        axisColor;
  Color        axisLabelColor;
  glm::vec3    axisScaling;
  glm::vec3    axisArrowScaling;
  unsigned int gridResolution;

  Impl (const Config& config)
  {
    this->runFromConfig (config);

    this->axisResolution = glm::uvec2 (200, 200);
    this->gridResolution = 6;

    this->coneMesh = MeshUtil::cone (10);
    this->cylinderMesh = MeshUtil::cylinder (10);

    this->cylinderMesh.renderMode ().constantShading (true);
    this->cylinderMesh.renderMode ().cameraRotationOnly (true);
    this->cylinderMesh.bufferData ();

    this->coneMesh.renderMode ().constantShading (true);
    this->coneMesh.renderMode ().cameraRotationOnly (true);
    this->coneMesh.bufferData ();

    this->initializeGrid ();
  }

  void initializeGrid ()
  {
    float gridSpace = 1.0f / float(gridResolution - 1);

    for (unsigned int j = 0; j < gridResolution; j++)
    {
      for (unsigned int i = 0; i < gridResolution; i++)
      {
        this->gridMesh.addVertex (glm::vec3 (float(i) * gridSpace, float(j) * gridSpace, 0.0f));
      }
    }
    for (unsigned int j = 1; j < gridResolution; j++)
    {
      for (unsigned int i = 1; i < gridResolution; i++)
      {
        this->gridMesh.addIndex ((j * gridResolution) + i - 1);
        this->gridMesh.addIndex ((j * gridResolution) + i);
        this->gridMesh.addIndex (((j - 1) * gridResolution) + i);
        this->gridMesh.addIndex ((j * gridResolution) + i);
      }
    }
    this->gridMesh.renderMode ().constantShading (true);
    this->gridMesh.renderMode ().cameraRotationOnly (true);
    this->gridMesh.bufferData ();
  }

  void render (Camera& camera)
  {
    OpenGL::glClear (OpenGL::DepthBufferBit ());

    const glm::uvec2 resolution = camera.resolution ();
    camera.updateResolution (glm::uvec2 (200, 200));

    this->cylinderMesh.scaling (this->axisScaling);

    this->cylinderMesh.position (glm::vec3 (0.0f, this->axisScaling.y * 0.5f, 0.0f));
    this->cylinderMesh.rotationMatrix (glm::mat4x4 (1.0f));
    this->cylinderMesh.color (this->axisColor);
    this->cylinderMesh.render (camera);

    this->cylinderMesh.position (glm::vec3 (this->axisScaling.y * 0.5f, 0.0f, 0.0f));
    this->cylinderMesh.rotationZ (0.5f * glm::pi<float> ());
    this->cylinderMesh.render (camera);

    this->cylinderMesh.position (glm::vec3 (0.0f, 0.0f, this->axisScaling.y * 0.5f));
    this->cylinderMesh.rotationX (0.5f * glm::pi<float> ());
    this->cylinderMesh.render (camera);

    this->coneMesh.scaling (this->axisArrowScaling);

    this->coneMesh.position (glm::vec3 (0.0f, this->axisScaling.y, 0.0f));
    this->coneMesh.rotationMatrix (glm::mat4x4 (1.0f));
    this->coneMesh.color (this->axisColor);
    this->coneMesh.render (camera);

    this->coneMesh.position (glm::vec3 (this->axisScaling.y, 0.0f, 0.0f));
    this->coneMesh.rotationZ (-0.5f * glm::pi<float> ());
    this->coneMesh.render (camera);

    this->coneMesh.position (glm::vec3 (0.0f, 0.0f, this->axisScaling.y));
    this->coneMesh.rotationX (0.5f * glm::pi<float> ());
    this->coneMesh.render (camera);

    this->renderGrid (camera);

    camera.updateResolution (resolution);
  }

  void renderGrid (Camera& camera)
  {
    switch (camera.primaryDimension ())
    {
      case Dimension::X:
        this->gridMesh.rotationY (-0.5f * glm::pi<float> ());
        break;
      case Dimension::Y:
        this->gridMesh.rotationX (0.5f * glm::pi<float> ());
        break;
      case Dimension::Z:
        this->gridMesh.rotationMatrix (glm::mat4x4 (1.0f));
        break;
    }
    this->gridMesh.scaling (glm::vec3 (this->axisScaling.y));
    this->gridMesh.color (this->axisColor);
    this->gridMesh.renderLines (camera);
  }

  void render (Camera& camera, QPainter& painter)
  {
    this->coneMesh.rotationMatrix (glm::mat4x4 (1.0f));

    QFont font;
    font.setWeight (QFont::Bold);

    QFontMetrics metrics (font);
    int          w = glm::max (metrics.maxWidth (), metrics.height ());
    glm::uvec2   resolution = camera.resolution ();
    camera.updateResolution (this->axisResolution);

    auto renderLabel = [this, &resolution, &painter, w, &camera](const glm::vec3& p,
                                                                 const QString&   l) {
      this->coneMesh.position (p);

      glm::vec2 pos = camera.fromWorld (glm::vec3 (0.0f), this->coneMesh.modelMatrix (), true);
      QRect     rect (int(pos.x) - (w / 2),
                      resolution.y - this->axisResolution.y + int(pos.y) - (w / 2), w, w);

      painter.drawText (rect, Qt::AlignCenter, l);
    };

    painter.setPen (this->axisLabelColor.qColor ());
    painter.setFont (font);

    const float labelPosition = this->axisScaling.y + (this->axisArrowScaling.y * 0.5f);

    renderLabel (glm::vec3 (labelPosition, 0.0f, 0.0f), "X");
    renderLabel (glm::vec3 (0.0f, labelPosition, 0.0f), "Y");
    renderLabel (glm::vec3 (0.0f, 0.0f, labelPosition), "Z");

    camera.updateResolution (resolution);
  }

  void runFromConfig (const Config& config)
  {
    this->axisLabelColor = config.get<Color> ("editor/axis/color/label");
    this->axisColor = config.get<Color> ("editor/axis/color/normal");
    this->axisScaling = config.get<glm::vec3> ("editor/axis/scaling");
    this->axisArrowScaling = config.get<glm::vec3> ("editor/axis/arrow-scaling");
  }
};

DELEGATE1_BIG3 (ViewAxis, const Config&)
DELEGATE1 (void, ViewAxis, render, Camera&)
DELEGATE2 (void, ViewAxis, render, Camera&, QPainter&)
DELEGATE1 (void, ViewAxis, runFromConfig, const Config&)
