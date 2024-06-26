// Copyright 2024 Tier IV, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "continental_ros_decoder_test_ars548.hpp"

#include "rclcpp/serialization.hpp"
#include "rclcpp/serialized_message.hpp"
#include "rcpputils/filesystem_helper.hpp"
#include "rcutils/time.h"
#include "rosbag2_cpp/reader.hpp"
#include "rosbag2_cpp/readers/sequential_reader.hpp"
#include "rosbag2_cpp/writer.hpp"
#include "rosbag2_cpp/writers/sequential_writer.hpp"
#include "rosbag2_storage/storage_options.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <regex>
#include <string>

namespace nebula
{
namespace ros
{
ContinentalRosDecoderTest::ContinentalRosDecoderTest(
  const rclcpp::NodeOptions & options, const std::string & node_name)
: rclcpp::Node(node_name, options)
{
  drivers::continental_ars548::ContinentalARS548SensorConfiguration sensor_configuration;

  wrapper_status_ = GetParameters(sensor_configuration);
  if (Status::OK != wrapper_status_) {
    RCLCPP_ERROR_STREAM(this->get_logger(), this->get_name() << " Error:" << wrapper_status_);
    return;
  }
  RCLCPP_INFO_STREAM(this->get_logger(), this->get_name() << ". Starting...");

  sensor_cfg_ptr_ =
    std::make_shared<drivers::continental_ars548::ContinentalARS548SensorConfiguration>(
      sensor_configuration);

  RCLCPP_INFO_STREAM(this->get_logger(), this->get_name() << ". Driver ");
  wrapper_status_ = InitializeDriver(
    std::const_pointer_cast<drivers::continental_ars548::ContinentalARS548SensorConfiguration>(
      sensor_cfg_ptr_));

  driver_ptr_->RegisterDetectionListCallback(
    std::bind(&ContinentalRosDecoderTest::DetectionListCallback, this, std::placeholders::_1));
  driver_ptr_->RegisterObjectListCallback(
    std::bind(&ContinentalRosDecoderTest::ObjectListCallback, this, std::placeholders::_1));

  RCLCPP_INFO_STREAM(this->get_logger(), this->get_name() << "Wrapper=" << wrapper_status_);
}

Status ContinentalRosDecoderTest::InitializeDriver(
  std::shared_ptr<drivers::continental_ars548::ContinentalARS548SensorConfiguration>
    sensor_configuration)
{
  // driver should be initialized here with proper decoder
  driver_ptr_ = std::make_shared<drivers::continental_ars548::ContinentalARS548Decoder>(
    std::static_pointer_cast<drivers::continental_ars548::ContinentalARS548SensorConfiguration>(
      sensor_configuration));
  return Status::OK;
}

Status ContinentalRosDecoderTest::GetStatus()
{
  return wrapper_status_;
}

Status ContinentalRosDecoderTest::GetParameters(
  drivers::continental_ars548::ContinentalARS548SensorConfiguration & sensor_configuration)
{
  std::filesystem::path bag_root_dir =
    _SRC_RESOURCES_DIR_PATH;  // variable defined in CMakeLists.txt;
  bag_root_dir /= "continental";
  {
    rcl_interfaces::msg::ParameterDescriptor descriptor;
    descriptor.type = 4;
    descriptor.read_only = true;
    descriptor.dynamic_typing = false;
    descriptor.additional_constraints = "";
    this->declare_parameter<std::string>("sensor_model", "ARS548");
    sensor_configuration.sensor_model =
      nebula::drivers::SensorModelFromString(this->get_parameter("sensor_model").as_string());
  }
  {
    rcl_interfaces::msg::ParameterDescriptor descriptor;
    descriptor.type = 4;
    descriptor.read_only = true;
    descriptor.dynamic_typing = false;
    descriptor.additional_constraints = "";
    this->declare_parameter<std::string>("base_frame", "some_base_frame", descriptor);
    sensor_configuration.base_frame = this->get_parameter("base_frame").as_string();
  }
  {
    rcl_interfaces::msg::ParameterDescriptor descriptor;
    descriptor.type = 4;
    descriptor.read_only = true;
    descriptor.dynamic_typing = false;
    descriptor.additional_constraints = "";
    this->declare_parameter<std::string>("object_frame", "some_object_frame", descriptor);
    sensor_configuration.object_frame = this->get_parameter("object_frame").as_string();
  }
  {
    rcl_interfaces::msg::ParameterDescriptor descriptor;
    descriptor.type = 4;
    descriptor.read_only = true;
    descriptor.dynamic_typing = false;
    descriptor.additional_constraints = "";
    this->declare_parameter<std::string>("frame_id", "some_sensor_frame", descriptor);
    sensor_configuration.frame_id = this->get_parameter("frame_id").as_string();
  }
  {
    rcl_interfaces::msg::ParameterDescriptor descriptor;
    descriptor.type = 4;
    descriptor.read_only = true;
    descriptor.dynamic_typing = false;
    descriptor.additional_constraints = "";
    this->declare_parameter<std::string>(
      "bag_path", (bag_root_dir / "ars548" / "1708578204").string(), descriptor);
    bag_path = this->get_parameter("bag_path").as_string();
    std::cout << bag_path << std::endl;
  }
  {
    rcl_interfaces::msg::ParameterDescriptor descriptor;
    descriptor.type = 4;
    descriptor.read_only = true;
    descriptor.dynamic_typing = false;
    descriptor.additional_constraints = "";
    this->declare_parameter<std::string>("storage_id", "sqlite3", descriptor);
    storage_id = this->get_parameter("storage_id").as_string();
  }
  {
    rcl_interfaces::msg::ParameterDescriptor descriptor;
    descriptor.type = 4;
    descriptor.read_only = true;
    descriptor.dynamic_typing = false;
    descriptor.additional_constraints = "";
    this->declare_parameter<std::string>("format", "cdr", descriptor);
    format = this->get_parameter("format").as_string();
  }
  {
    rcl_interfaces::msg::ParameterDescriptor descriptor;
    descriptor.type = 4;
    descriptor.read_only = true;
    descriptor.dynamic_typing = false;
    descriptor.additional_constraints = "";
    this->declare_parameter<std::string>(
      "target_topic", "/sensing/radar/front_center/nebula_packets", descriptor);
    target_topic = this->get_parameter("target_topic").as_string();
  }

  if (sensor_configuration.sensor_model == nebula::drivers::SensorModel::UNKNOWN) {
    return Status::INVALID_SENSOR_MODEL;
  }

  RCLCPP_INFO_STREAM(this->get_logger(), "SensorConfig:" << sensor_configuration);
  return Status::OK;
}

void ContinentalRosDecoderTest::CompareNodes(const YAML::Node & node1, const YAML::Node & node2)
{
  ASSERT_EQ(node1.IsDefined(), node2.IsDefined());
  ASSERT_EQ(node1.IsMap(), node2.IsMap());
  ASSERT_EQ(node1.IsNull(), node2.IsNull());
  ASSERT_EQ(node1.IsScalar(), node2.IsScalar());
  ASSERT_EQ(node1.IsSequence(), node2.IsSequence());

  if (node1.IsMap()) {
    for (YAML::const_iterator it = node1.begin(); it != node1.end(); ++it) {
      CompareNodes(it->second, node2[it->first.as<std::string>()]);
    }
  } else if (node1.IsScalar()) {
    ASSERT_EQ(node1.as<std::string>(), node2.as<std::string>());
  } else if (node1.IsSequence()) {
    ASSERT_EQ(node1.size(), node2.size());
    for (std::size_t i = 0; i < node1.size(); i++) {
      CompareNodes(node1[i], node2[i]);
    }
  }
}

void ContinentalRosDecoderTest::CheckResult(
  const std::string msg_as_string, const std::string & gt_path)
{
  YAML::Node current_node = YAML::Load(msg_as_string);
  YAML::Node gt_node = YAML::LoadFile(gt_path);
  CompareNodes(gt_node, current_node);

  // To generate the gt
  // std::ofstream ostream(gt_path);
  // ostream << msg_as_string;
  // ostream.close();
}

void ContinentalRosDecoderTest::DetectionListCallback(
  std::unique_ptr<continental_msgs::msg::ContinentalArs548DetectionList> msg)
{
  EXPECT_EQ(sensor_cfg_ptr_->frame_id, msg->header.frame_id);
  std::string msg_as_string = continental_msgs::msg::to_yaml(*msg);

  std::stringstream detection_path;
  detection_path << msg->header.stamp.sec << "_" << msg->header.stamp.nanosec << "_detection.yaml";

  auto gt_path = rcpputils::fs::path(bag_path).parent_path() / detection_path.str();
  ASSERT_TRUE(gt_path.exists());

  CheckResult(msg_as_string, gt_path.string());
}

void ContinentalRosDecoderTest::ObjectListCallback(
  std::unique_ptr<continental_msgs::msg::ContinentalArs548ObjectList> msg)
{
  EXPECT_EQ(sensor_cfg_ptr_->object_frame, msg->header.frame_id);
  std::string msg_as_string = continental_msgs::msg::to_yaml(*msg);

  std::stringstream detection_path;
  detection_path << msg->header.stamp.sec << "_" << msg->header.stamp.nanosec << "_object.yaml";

  auto gt_path = rcpputils::fs::path(bag_path).parent_path() / detection_path.str();
  ASSERT_TRUE(gt_path.exists());

  CheckResult(msg_as_string, gt_path.string());
}

void ContinentalRosDecoderTest::ReadBag()
{
  rosbag2_storage::StorageOptions storage_options;
  rosbag2_cpp::ConverterOptions converter_options;

  std::cout << bag_path << std::endl;
  std::cout << storage_id << std::endl;
  std::cout << format << std::endl;
  std::cout << target_topic << std::endl;

  auto target_topic_name = target_topic;
  if (target_topic_name.substr(0, 1) == "/") {
    target_topic_name = target_topic_name.substr(1);
  }
  target_topic_name = std::regex_replace(target_topic_name, std::regex("/"), "_");

  rcpputils::fs::path bag_dir(bag_path);

  storage_options.uri = bag_path;
  storage_options.storage_id = storage_id;
  converter_options.output_serialization_format = format;  // "cdr";
  rclcpp::Serialization<nebula_msgs::msg::NebulaPackets> serialization;

  {
    rosbag2_cpp::Reader bag_reader(std::make_unique<rosbag2_cpp::readers::SequentialReader>());
    bag_reader.open(storage_options, converter_options);
    while (bag_reader.has_next()) {
      auto bag_message = bag_reader.read_next();

      std::cout << "Found topic name " << bag_message->topic_name << std::endl;

      if (bag_message->topic_name == target_topic) {
        nebula_msgs::msg::NebulaPackets extracted_msg;
        rclcpp::SerializedMessage extracted_serialized_msg(*bag_message->serialized_data);
        serialization.deserialize_message(&extracted_serialized_msg, &extracted_msg);

        std::cout << "Found data in topic " << bag_message->topic_name << ": "
                  << bag_message->time_stamp << std::endl;

        ASSERT_EQ(1, extracted_msg.packets.size());

        auto extracted_msg_ptr = std::make_shared<nebula_msgs::msg::NebulaPackets>(extracted_msg);
        driver_ptr_->ProcessPackets(extracted_msg);
      }
    }
  }
}

}  // namespace ros
}  // namespace nebula
