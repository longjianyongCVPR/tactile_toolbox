/*
 * Copyright (C) 2016, Bielefeld University, CITEC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Willow Garage, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Robert Haschke <rhaschke@techfak.uni-bielefeld.de>
 */

#include "tactile_state_display.h"
#include "tactile_taxels_visual.h"
#include "tactile_array_visual.h"

#include <urdf_parser/urdf_parser.h>
#include <urdf_sensor/tactile.h>

#include <rviz/visualization_manager.h>
#include <rviz/frame_manager.h>
#include <rviz/properties/color_property.h>
#include <rviz/properties/float_property.h>
#include <rviz/properties/int_property.h>
#include <rviz/properties/parse_color.h>
#include <rviz/properties/ros_topic_property.h>
#include <rviz/validate_floats.h>

#include <boost/foreach.hpp>
const QString ROBOT_DESC = "robot description";

namespace rviz
{
namespace tactile {

TactileStateDisplay::TactileStateDisplay() {
  topic_property_ = new rviz::RosTopicProperty
      ("topic", "/tactile_state", "tactile_msgs/TactileState", "",
       this, SLOT(onTopicChanged()));

  robot_description_property_ = new rviz::StringProperty
      (ROBOT_DESC, "robot_description",
       ROBOT_DESC + " defining tactile sensors",
       this, SLOT(onRobotDescriptionChanged()));

  timeout_property_ = new rviz::FloatProperty
      ("display timeout", 1, "", this);

  sensors_property_ = new rviz::BoolProperty("sensors", true, "", this,
                                             SLOT(onAllVisibleChanged()));
  sensors_property_->collapse();

  // init color maps
  QStringList colorNames;
  abs_color_map_.init(0,1);
  colorNames << "black" << "lime" << "yellow" << "red";
  abs_color_map_.append(colorNames);

  rel_color_map_.init(-1,1);
  colorNames.clear(); colorNames << "red" << "black" << "lime";
  rel_color_map_.append(colorNames);
}

TactileStateDisplay::~TactileStateDisplay()
{
  unsubscribe();
}

void TactileStateDisplay::subscribe()
{
  if (!isEnabled() ||
      topic_property_->getTopicStd().empty() ||
      sensors_.empty())
    return;

  try {
    sub_ = nh_.subscribe(topic_property_->getTopicStd(), 10,
                         &TactileStateDisplay::processMessage, this);
    setStatus(StatusProperty::Ok, "topic", "OK");
  } catch(const ros::Exception& e) {
    setStatus(StatusProperty::Error, "topic", QString("error subscribing: ") + e.what());
  }
}

void TactileStateDisplay::unsubscribe()
{
  sub_.shutdown();
}

void TactileStateDisplay::onInitialize()
{
  onRobotDescriptionChanged();
}

void TactileStateDisplay::reset()
{
  Display::reset();
}

void TactileStateDisplay::onEnable()
{
  subscribe();
}

void TactileStateDisplay::onDisable()
{
  unsubscribe();
  reset();
}

void TactileStateDisplay::onTopicChanged()
{
  unsubscribe();
  reset();
  subscribe();
  context_->queueRender();
}

void TactileStateDisplay::onRobotDescriptionChanged()
{
  std::string xml_string;

  sensors_.clear();

  // read the robot description from the parameter server
  const QString &param = robot_description_property_->getString();
  if (!nh_.getParam(param.toStdString(), xml_string)) {
    setStatus(rviz::StatusProperty::Error, ROBOT_DESC,
              QString("failed to read %1% from parameter server").arg(param));
    return;
  }

  boost::shared_ptr<urdf::ModelInterface> urdf_model = urdf::parseURDF(xml_string);
  if (!urdf_model) {
    setStatus(rviz::StatusProperty::Error, ROBOT_DESC,
              QString("failed to parse URDF from ") + param);
    return;
  }

  // create a TactileVisual for each tactile sensor listed in the URDF model
  for (auto it = urdf_model->sensors_.begin(), end = urdf_model->sensors_.end(); it != end; ++it)
  {
    boost::shared_ptr<urdf::Tactile> sensor = boost::dynamic_pointer_cast<urdf::Tactile>(it->second->sensor);
    if (!sensor) continue;  // some other sensor than tactile

    TactileVisualBasePtr visual;
    if (sensor->_array) {
      visual.reset(new TactileArrayVisual(it->first, it->second->parent_link_name, sensor->_array,
                                          this, context_, scene_node_, sensors_property_));
    } else if (sensor->_taxels.size()) {
      visual.reset(new TactileTaxelsVisual(it->first, it->second->parent_link_name, sensor->_taxels,
                                           this, context_, scene_node_, sensors_property_));
    }
    if (visual) {
      // visual->setMode();
      visual->setColorMap(&abs_color_map_);
      sensors_[it->first] = visual;
    }
  }

  subscribe();
  context_->queueRender();
}

void TactileStateDisplay::onAllVisibleChanged()
{
  bool show = sensors_property_->getBool();
  for (auto it = sensors_.begin(), end = sensors_.end(); it != end; ++it)
    it->second->setVisible(show);
}

// This is our callback to handle an incoming message.
void TactileStateDisplay::processMessage(const tactile_msgs::TactileState::ConstPtr& msg)
{
  for (auto sensor = msg->sensors.begin(), end = msg->sensors.end(); sensor != end; ++sensor)
  {
    auto it = sensors_.find(sensor->name);
    if (it == sensors_.end()) continue;
    it->second->update(msg->header.stamp, sensor->values);
  }
}

void TactileStateDisplay::update(float wall_dt, float ros_dt)
{
  if (!this->isEnabled()) return;

  Display::update(wall_dt, ros_dt);

  ros::Time timeout = ros::Time::now();
  try {
    timeout -= ros::Duration(timeout_property_->getFloat());
  } catch (const std::runtime_error &e) {
    // ros::Time::now was smaller than ros::Duration
  }

  for (auto it = sensors_.begin(), end = sensors_.end(); it != end; ++it) {
    TactileVisualBase &sensor = *it->second;
    sensor.setEnabled(sensor.expired(timeout) == false);
    if (!sensor.isEnabled() || !sensor.isVisible()) continue;

    sensor.update();
  }
}

} // end namespace tactile
} // end namespace rviz
