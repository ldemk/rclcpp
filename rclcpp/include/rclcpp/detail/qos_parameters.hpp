// Copyright 2020 Open Source Robotics Foundation, Inc.
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

#ifndef RCLCPP__DETAIL__QOS_PARAMETERS_HPP_
#define RCLCPP__DETAIL__QOS_PARAMETERS_HPP_

#include <algorithm>
#include <array>
#include <functional>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

#include "rcl_interfaces/msg/parameter_descriptor.hpp"

#include "rmw/qos_string_conversions.h"

#include "rclcpp/duration.hpp"
#include "rclcpp/node_interfaces/node_parameters_interface.hpp"
#include "rclcpp/qos_overriding_options.hpp"

namespace rclcpp
{
namespace detail
{

/// \internal Trait used to specialize `declare_qos_parameters()` for publishers.
struct PublisherQosParametersTraits
{
  static constexpr const char * entity_type() {return "publisher";}
  static constexpr auto allowed_policies()
  {
    return std::array<::rclcpp::QosPolicyKind, 9> {
      QosPolicyKind::AvoidRosNamespaceConventions,
      QosPolicyKind::Deadline,
      QosPolicyKind::Durability,
      QosPolicyKind::History,
      QosPolicyKind::Depth,
      QosPolicyKind::Lifespan,
      QosPolicyKind::Liveliness,
      QosPolicyKind::LivelinessLeaseDuration,
      QosPolicyKind::Reliability,
    };
  }
};

/// \internal Trait used to specialize `declare_qos_parameters()` for subscriptions.
struct SubscriptionQosParametersTraits
{
  static constexpr const char * entity_type() {return "subscription";}
  static constexpr auto allowed_policies()
  {
    return std::array<::rclcpp::QosPolicyKind, 8> {
      QosPolicyKind::AvoidRosNamespaceConventions,
      QosPolicyKind::Deadline,
      QosPolicyKind::Durability,
      QosPolicyKind::History,
      QosPolicyKind::Depth,
      QosPolicyKind::Liveliness,
      QosPolicyKind::LivelinessLeaseDuration,
      QosPolicyKind::Reliability,
    };
  }
};

/// \internal Declare qos parameters for the given entity.
/**
 * \tparam EntityQosParametersTraits A class with two static methods: `entity_type()` and
 *  `allowed_policies()`. See `PublisherQosParametersTraits` and `SubscriptionQosParametersTraits`.
 * \param options User provided options that indicate if qos parameter overrides should be
 *  declared or not, which policy can have overrides, and optionally a callback to validate the profile.
 * \param parameters_interface Parameters will be declared through this interface.
 * \param topic_name Name of the topic of the entity.
 * \param qos User provided qos. It will be used as a default for the parameters declared,
 *  and then overriden with the final parameter overrides.
 */
template<typename EntityQosParametersTraits>
inline
void declare_qos_parameters(
  const ::rclcpp::QosOverridingOptions & options,
  ::rclcpp::node_interfaces::NodeParametersInterface & parameters_interface,
  const std::string & topic_name,
  ::rclcpp::QoS & qos,
  EntityQosParametersTraits);

/// \internal Same as `declare_qos_parameters()` for a `Publisher`.
inline
void
declare_publisher_qos_parameters(
  const ::rclcpp::QosOverridingOptions & options,
  ::rclcpp::node_interfaces::NodeParametersInterface & parameters_interface,
  const std::string & topic_name,
  ::rclcpp::QoS & qos)
{
  declare_qos_parameters(
    options, parameters_interface, topic_name, qos, PublisherQosParametersTraits{});
}

/// \internal Same as `declare_qos_parameters()` for a `Subscription`.
inline
void
declare_subscription_qos_parameters(
  const ::rclcpp::QosOverridingOptions & options,
  ::rclcpp::node_interfaces::NodeParametersInterface & parameters_interface,
  const std::string & topic_name,
  ::rclcpp::QoS & qos)
{
  declare_qos_parameters(
    options, parameters_interface, topic_name, qos, SubscriptionQosParametersTraits{});
}

/// \internal Returns the given `policy` of the profile `qos` converted to a parameter value.
inline
::rclcpp::ParameterValue
get_default_qos_param_value(rclcpp::QosPolicyKind policy, const rclcpp::QoS & qos);

/// \internal Modify the given `policy` in `qos` to be `value`.
inline
void
apply_qos_override(
  rclcpp::QosPolicyKind policy, rclcpp::ParameterValue value, rclcpp::QoS & qos);

template<typename EntityQosParametersTraits>
inline
void declare_qos_parameters(
  const ::rclcpp::QosOverridingOptions & options,
  ::rclcpp::node_interfaces::NodeParametersInterface & parameters_interface,
  const std::string & topic_name,
  ::rclcpp::QoS & qos,
  EntityQosParametersTraits)
{
  std::string param_prefix;
  {
    std::ostringstream oss{"qos_overrides.", std::ios::ate};
    oss << topic_name << "." << EntityQosParametersTraits::entity_type();
    if (!options.id.empty()) {
      oss << "_" << options.id;
    }
    oss << ".";
    param_prefix = oss.str();
  }
  std::string param_description_suffix;
  {
    std::ostringstream oss{"} for ", std::ios::ate};
    oss << EntityQosParametersTraits::entity_type() << " {" << topic_name << "}";
    if (!options.id.empty()) {
      oss << " with id {" << options.id << "}";
    }
    param_description_suffix = oss.str();
  }
  for (auto policy : EntityQosParametersTraits::allowed_policies()) {
    if (
      std::count(options.policy_kinds.begin(), options.policy_kinds.end(), policy))
    {
      std::ostringstream param_name{param_prefix, std::ios::ate};
      param_name << qos_policy_kind_to_cstr(policy);
      std::ostringstream param_desciption{"qos policy {", std::ios::ate};
      param_desciption << qos_policy_kind_to_cstr(policy) << param_description_suffix;
      rcl_interfaces::msg::ParameterDescriptor descriptor{};
      descriptor.description = param_desciption.str();
      descriptor.read_only = true;
      auto value = parameters_interface.declare_parameter(
        param_name.str(), get_default_qos_param_value(policy, qos), descriptor);
      ::rclcpp::detail::apply_qos_override(policy, value, qos);
    }
  }
  if (options.validation_callback && !options.validation_callback(qos)) {
    throw rclcpp::exceptions::InvalidQosOverridesException{"validation callback failed"};
  }
}

/// \internal Helper function to get a rmw qos policy value from a string.
#define RCLCPP_DETAIL_QOS_POLICY_FROM_PARAMETER_STRING( \
    kind_lower, kind_upper, parameter_value, rclcpp_qos) \
  do { \
    auto policy_string = (parameter_value).get<std::string>(); \
    auto policy_value = rmw_qos_ ## kind_lower ## _policy_from_str(policy_string.c_str()); \
    if (RMW_QOS_POLICY_ ## kind_upper ## _UNKNOWN == policy_value) { \
      throw std::invalid_argument{"unknown qos policy " #kind_lower " value: " + policy_string}; \
    } \
    ((rclcpp_qos).kind_lower)(policy_value); \
  } while (0)

inline
void
apply_qos_override(
  rclcpp::QosPolicyKind policy, rclcpp::ParameterValue value, rclcpp::QoS & qos)
{
  switch (policy) {
    case QosPolicyKind::AvoidRosNamespaceConventions:
      qos.avoid_ros_namespace_conventions(value.get<bool>());
      break;
    case QosPolicyKind::Deadline:
      qos.deadline(::rclcpp::Duration(value.get<int64_t>()));
      break;
    case QosPolicyKind::Durability:
      RCLCPP_DETAIL_QOS_POLICY_FROM_PARAMETER_STRING(
        durability, DURABILITY, value, qos);
      break;
    case QosPolicyKind::History:
      RCLCPP_DETAIL_QOS_POLICY_FROM_PARAMETER_STRING(
        history, HISTORY, value, qos);
      break;
    case QosPolicyKind::Depth:
      qos.get_rmw_qos_profile().depth = static_cast<size_t>(value.get<int64_t>());
      break;
    case QosPolicyKind::Lifespan:
      qos.lifespan(::rclcpp::Duration(value.get<int64_t>()));
      break;
    case QosPolicyKind::Liveliness:
      RCLCPP_DETAIL_QOS_POLICY_FROM_PARAMETER_STRING(
        liveliness, LIVELINESS, value, qos);
      break;
    case QosPolicyKind::LivelinessLeaseDuration:
      qos.liveliness_lease_duration(::rclcpp::Duration(value.get<int64_t>()));
      break;
    case QosPolicyKind::Reliability:
      RCLCPP_DETAIL_QOS_POLICY_FROM_PARAMETER_STRING(
        reliability, RELIABILITY, value, qos);
      break;
    default:
      throw std::runtime_error{"unknown QosPolicyKind"};
  }
}

/// Convert `rmw_time_t` to `int64_t` that can be used as a parameter value.
inline
int64_t
rmw_duration_to_int64_t(rmw_time_t rmw_duration)
{
  return ::rclcpp::Duration(
    static_cast<int32_t>(rmw_duration.sec),
    static_cast<uint32_t>(rmw_duration.nsec)
  ).nanoseconds();
}

/// \internal Throw an exception if `policy_value_stringified` is NULL.
inline
const char *
check_if_stringified_policy_is_null(const char * policy_value_stringified, QosPolicyKind kind)
{
  if (!policy_value_stringified) {
    std::ostringstream oss{"unknown ", std::ios::ate};
    oss << kind << " qos policy value: {" << policy_value_stringified << "}";
    throw std::invalid_argument{oss.str()};
  }
  return policy_value_stringified;
}

inline
::rclcpp::ParameterValue
get_default_qos_param_value(rclcpp::QosPolicyKind qpk, const rclcpp::QoS & qos)
{
  using ParameterValue = ::rclcpp::ParameterValue;
  const auto & rmw_qos = qos.get_rmw_qos_profile();
  switch (qpk) {
    case QosPolicyKind::AvoidRosNamespaceConventions:
      return ParameterValue(rmw_qos.avoid_ros_namespace_conventions);
    case QosPolicyKind::Deadline:
      return ParameterValue(rmw_duration_to_int64_t(rmw_qos.deadline));
    case QosPolicyKind::Durability:
      return ParameterValue(
        check_if_stringified_policy_is_null(
          rmw_qos_durability_policy_to_str(rmw_qos.durability), qpk));
    case QosPolicyKind::History:
      return ParameterValue(
        check_if_stringified_policy_is_null(
          rmw_qos_history_policy_to_str(rmw_qos.history), qpk));
    case QosPolicyKind::Depth:
      return ParameterValue(static_cast<int64_t>(rmw_qos.depth));
    case QosPolicyKind::Lifespan:
      return ParameterValue(rmw_duration_to_int64_t(rmw_qos.lifespan));
    case QosPolicyKind::Liveliness:
      return ParameterValue(
        check_if_stringified_policy_is_null(
          rmw_qos_liveliness_policy_to_str(rmw_qos.liveliness), qpk));
    case QosPolicyKind::LivelinessLeaseDuration:
      return ParameterValue(rmw_duration_to_int64_t(rmw_qos.liveliness_lease_duration));
    case QosPolicyKind::Reliability:
      return ParameterValue(
        check_if_stringified_policy_is_null(
          rmw_qos_reliability_policy_to_str(rmw_qos.reliability), qpk));
    default:
      throw std::invalid_argument{"unknown qos policy kind"};
  }
}

}  // namespace detail
}  // namespace rclcpp

#endif  // RCLCPP__DETAIL__QOS_PARAMETERS_HPP_
