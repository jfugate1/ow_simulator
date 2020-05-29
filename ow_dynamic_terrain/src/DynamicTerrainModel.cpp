#include <ros/callback_queue.h>
#include <ros/ros.h>
#include <ros/subscribe_options.h>
#include <gazebo/common/common.hh>
#include <gazebo/physics/physics.hh>
#include <gazebo/rendering/RenderingIface.hh>
#include <gazebo/rendering/Scene.hh>
#include "TerrainModifier.h"
#include "ow_dynamic_terrain/modify_terrain_circle.h"
#include "ow_dynamic_terrain/modify_terrain_patch.h"

using namespace std;
using namespace gazebo;

namespace ow_dynamic_terrain
{
class DynamicTerrainModel : public ModelPlugin
{
public:
  void Load(physics::ModelPtr model, sdf::ElementPtr /*sdf*/) override
  {
    if (!ros::isInitialized())
    {
      gzerr << "DynamicTerrainModel: ROS not initilized!" << endl;
      return;
    }

    m_model = model;

    m_ros_node.reset(new ros::NodeHandle("dynamic_terrain_model"));
    m_ros_node->setCallbackQueue(&m_ros_queue);

    m_ros_subscriber_circle = m_ros_node->subscribe<modify_terrain_circle>(
        "/ow_dynamic_terrain/modify_terrain_circle", 10,
        boost::bind(&DynamicTerrainModel::onModifyTerrainCircleMsg, this, _1));

    m_ros_subscriber_patch = m_ros_node->subscribe<modify_terrain_patch>(
        "/ow_dynamic_terrain/modify_terrain_patch", 10,
        boost::bind(&DynamicTerrainModel::onModifyTerrainPatchMsg, this, _1));

    m_on_update_connection = event::Events::ConnectPostRender(bind(&DynamicTerrainModel::onUpdate, this));

    gzlog << "DynamicTerrainModel: successfully loaded!" << endl;
  }

private:
  rendering::Heightmap* getHeightmap()
  {
    auto scene = rendering::get_scene();
    if (!scene)
    {
      gzerr << "DynamicTerrainModel: Couldn't acquire scene!" << endl;
      return nullptr;
    }

    auto heightmap = scene->GetHeightmap();
    if (heightmap == nullptr)
    {
      gzerr << "DynamicTerrainModel: scene has no heightmap!" << endl;
      return nullptr;
    }

    return heightmap;
  }

private:
  physics::HeightmapShapePtr getHeightmapShape()
  {
    if (m_model == nullptr)
    {
      gzerr << "DynamicTerrainModel: Couldn't acquire heightmap model!" << endl;
      return nullptr;
    }

    auto links = m_model->GetLinks();

    if (links.size() == 0)
    {
      gzerr << "DynamicTerrainModel: Associcated model has no links!" << endl;
      return nullptr;
    }

    auto link0 = links[0];

    auto collisions = link0->GetCollisions();

    if (collisions.size() == 0)
    {
      gzerr << "DynamicTerrainModel: Model has no collisions for first link!" << endl;
      return nullptr;
    }

    auto collision = collisions[0];
    if (collision == nullptr)
    {
      gzerr << "DynamicTerrainModel: Couldn't acquire heightmap model collision!" << endl;
      return nullptr;
    }

    auto shape = boost::dynamic_pointer_cast<physics::HeightmapShape>(collision->GetShape());
    if (shape == nullptr)
    {
      gzerr << "DynamicTerrainModel: Couldn't acquire heightmap model collision!" << endl;
      return nullptr;
    }

    return shape;
  }

private:
  void onUpdate()
  {
    if (m_ros_node->ok())
      m_ros_queue.callAvailable();
  }

private:
  void onModifyTerrainCircleMsg(const modify_terrain_circle::ConstPtr msg)
  {
    auto heightmap = getHeightmap();
    if (heightmap == nullptr)
    {
      gzerr << "DynamicTerrainModel: Couldn't acquire heightmap!" << endl;
      return;
    }

    auto heightmap_shape = getHeightmapShape();
    if (heightmap_shape == nullptr)
    {
      gzerr << "DynamicTerrainModel: Couldn't acquire heightmap shape!" << endl;
      return;
    }

#if GAZEBO_MAJOR_VERSION >= 9 && GAZEBO_MINOR_VERSION > 12
    TerrainModifier::modifyCircle(heightmap, msg,
                                  [&heightmap_shape](int x, int y) {
                                    return heightmap_shape->GetHeight(x, heightmap_shape->VertexCount().Y() - y - 1);
                                  },
                                  [&heightmap_shape](int x, int y, float value) {
                                    heightmap_shape->SetHeight(x, heightmap_shape->VertexCount().Y() - y - 1, value);
                                  });
#endif
  }

private:
  void onModifyTerrainPatchMsg(const modify_terrain_patch::ConstPtr msg)
  {
    auto heightmap = getHeightmap();
    if (heightmap == nullptr)
    {
      gzerr << "DynamicTerrainModel: Couldn't acquire heightmap!" << endl;
      return;
    }

    auto heightmap_shape = getHeightmapShape();
    if (heightmap_shape == nullptr)
    {
      gzerr << "DynamicTerrainModel: Couldn't acquire heightmap shape!" << endl;
      return;
    }

#if GAZEBO_MAJOR_VERSION >= 9 && GAZEBO_MINOR_VERSION > 12
    TerrainModifier::modifyPatch(heightmap, msg,
                                 [&heightmap_shape](int x, int y) {
                                   return heightmap_shape->GetHeight(x, heightmap_shape->VertexCount().Y() - y - 1);
                                 },
                                 [&heightmap_shape](int x, int y, float value) {
                                   heightmap_shape->SetHeight(x, heightmap_shape->VertexCount().Y() - y - 1, value);
                                 });
#endif
  }

private:
  physics::ModelPtr m_model;

private:
  event::ConnectionPtr m_on_update_connection;

private:
  unique_ptr<ros::NodeHandle> m_ros_node;

private:
  ros::CallbackQueue m_ros_queue;

private:
  ros::Subscriber m_ros_subscriber_circle;

private:
  ros::Subscriber m_ros_subscriber_patch;
};

// Register this plugin with the simulator
GZ_REGISTER_MODEL_PLUGIN(DynamicTerrainModel)
}  // namespace ow_dynamic_terrain