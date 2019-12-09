// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/feature_policy/document_policy.h"

#include <map>

#include "base/no_destructor.h"
#include "third_party/blink/public/common/http/structured_header.h"

namespace blink {

// static
std::unique_ptr<DocumentPolicy> DocumentPolicy::CreateWithRequiredPolicy(
    const FeatureState& required_policy) {
  return CreateWithRequiredPolicy(required_policy, GetFeatureDefaults());
}

namespace {
http_structured_header::Item PolicyValueToItem(const PolicyValue& value) {
  switch (value.Type()) {
    case mojom::PolicyValueType::kBool:
      return http_structured_header::Item{value.BoolValue()};
    case mojom::PolicyValueType::kDecDouble:
      return http_structured_header::Item{value.DoubleValue()};
    default:
      NOTREACHED();
      return http_structured_header::Item{
          nullptr, http_structured_header::Item::ItemType::kNullType};
  }
}

base::Optional<PolicyValue> ItemToPolicyValue(
    const http_structured_header::Item& item) {
  switch (item.Type()) {
    case http_structured_header::Item::ItemType::kIntegerType:
      return PolicyValue(static_cast<double>(item.GetInteger()));
    case http_structured_header::Item::ItemType::kFloatType:
      return PolicyValue(item.GetFloat());
    default:
      return base::nullopt;
  }
}

struct FeatureInfo {
  std::string feature_name;
  std::string feature_param_name;
};

using FeatureInfoMap = std::map<mojom::FeaturePolicyFeature, FeatureInfo>;

// TODO(iclelland): Generate this block
const FeatureInfoMap& GetDefaultFeatureInfoMap() {
  static base::NoDestructor<FeatureInfoMap> feature_info_map(
      {{mojom::FeaturePolicyFeature::kFontDisplay,
        {"font-display-late-swap", ""}},
       {mojom::FeaturePolicyFeature::kUnoptimizedLosslessImages,
        {"unoptimized-lossless-images", "bpp"}}});
  return *feature_info_map;
}

using NameFeatureMap = std::map<std::string, mojom::FeaturePolicyFeature>;

const NameFeatureMap& GetDefaultNameFeatureMap() {
  static base::NoDestructor<NameFeatureMap> name_feature_map;
  if (name_feature_map->empty()) {
    for (const auto& entry : GetDefaultFeatureInfoMap()) {
      (*name_feature_map)[entry.second.feature_name] = entry.first;
    }
  }
  return *name_feature_map;
}

}  // namespace

// static
base::Optional<std::string> DocumentPolicy::Serialize(
    const FeatureState& policy) {
  http_structured_header::List root;
  root.reserve(policy.size());

  std::vector<std::pair<mojom::FeaturePolicyFeature, PolicyValue>>
      sorted_policy(policy.begin(), policy.end());
  std::sort(sorted_policy.begin(), sorted_policy.end(),
            [](const auto& a, const auto& b) {
              const auto& m = GetDefaultFeatureInfoMap();
              const std::string& feature_a = m.at(a.first).feature_name;
              const std::string& feature_b = m.at(b.first).feature_name;
              return feature_a < feature_b;
            });

  for (const auto& policy_entry : sorted_policy) {
    const FeatureInfo& info =
        GetDefaultFeatureInfoMap().at(policy_entry.first /* feature */);

    const PolicyValue& value = policy_entry.second;
    if (value.Type() == mojom::PolicyValueType::kBool) {
      root.push_back(http_structured_header::ParameterizedMember(
          http_structured_header::Item(
              (value.BoolValue() ? "" : "no-") + info.feature_name,
              http_structured_header::Item::ItemType::kTokenType),
          {}));
    } else {
      http_structured_header::ParameterizedMember::Parameters params;
      params.push_back(std::pair<std::string, http_structured_header::Item>{
          info.feature_param_name, PolicyValueToItem(value)});
      root.push_back(http_structured_header::ParameterizedMember(
          http_structured_header::Item(
              info.feature_name,
              http_structured_header::Item::ItemType::kTokenType),
          params));
    }
  }

  return http_structured_header::SerializeList(root);
}

// static
base::Optional<DocumentPolicy::FeatureState> DocumentPolicy::Parse(
    const std::string& policy_string) {
  const auto& name_feature_map = GetDefaultNameFeatureMap();
  const auto& default_values_map = GetFeatureDefaults();
  const auto& feature_info_map = GetDefaultFeatureInfoMap();

  auto root = http_structured_header::ParseList(policy_string);
  if (!root)
    return base::nullopt;

  DocumentPolicy::FeatureState policy;
  for (const http_structured_header::ParameterizedMember& directive :
       root.value()) {
    // Each directive is allowed exactly 1 member.
    if (directive.member.size() != 1)
      return base::nullopt;

    const http_structured_header::Item& feature_token =
        directive.member.front();
    // The item in directive should be token type.
    if (!feature_token.is_token())
      return base::nullopt;

    // Feature policy now only support boolean and double PolicyValue
    // which correspond to 0 and 1 param number.
    if (directive.params.size() > 1)
      return base::nullopt;

    base::Optional<PolicyValue> policy_value;
    std::string feature_name = feature_token.GetString();

    if (directive.params.empty()) {  // boolean value
      // handle "no-" prefix
      const std::string& feature_str = feature_token.GetString();
      const bool bool_val =
          feature_str.size() < 3 || feature_str.substr(0, 3) != "no-";
      policy_value = PolicyValue(bool_val);
      if (!bool_val) {  // drop "no-" prefix
        feature_name = feature_name.substr(3);
      }
    } else {  // double value
      policy_value =
          ItemToPolicyValue(directive.params.front().second /* param value */);
    }

    if (!policy_value)
      return base::nullopt;

    if (name_feature_map.find(feature_name) ==
        name_feature_map.end())  // Unrecognized feature name.
      return base::nullopt;
    const mojom::FeaturePolicyFeature feature =
        name_feature_map.at(feature_name);

    if (default_values_map.at(feature).Type() !=
        policy_value->Type())  // Invalid value type.
      return base::nullopt;

    if ((*policy_value).Type() != mojom::PolicyValueType::kBool &&
        feature_info_map.at(feature).feature_param_name !=
            directive.params.front().first)  // Invalid param key name.
      return base::nullopt;

    policy.insert({feature, std::move(*policy_value)});
  }
  return policy;
}

bool DocumentPolicy::IsFeatureEnabled(
    mojom::FeaturePolicyFeature feature) const {
  mojom::PolicyValueType feature_type = GetFeatureDefaults().at(feature).Type();
  // TODO(iclelland): Generate this switch block
  switch (feature) {
    case mojom::FeaturePolicyFeature::kFontDisplay:
      return font_display_;
    case mojom::FeaturePolicyFeature::kUnoptimizedLosslessImages:
      return PolicyValue(unoptimized_lossless_images_) >=
             PolicyValue::CreateMaxPolicyValue(feature_type);
    default:
      NOTREACHED();
      return true;
  }
}

bool DocumentPolicy::IsFeatureEnabled(
    mojom::FeaturePolicyFeature feature,
    const PolicyValue& threshold_value) const {
  return GetFeatureValue(feature) >= threshold_value;
}

PolicyValue DocumentPolicy::GetFeatureValue(
    mojom::FeaturePolicyFeature feature) const {
  // TODO(iclelland): Generate this switch block
  switch (feature) {
    case mojom::FeaturePolicyFeature::kFontDisplay:
      return PolicyValue(font_display_);
    case mojom::FeaturePolicyFeature::kUnoptimizedLosslessImages:
      return PolicyValue(unoptimized_lossless_images_);
    default:
      NOTREACHED();
      return PolicyValue(false);
  }
}

bool DocumentPolicy::IsFeatureSupported(
    mojom::FeaturePolicyFeature feature) const {
  // TODO(iclelland): Generate this switch block
  switch (feature) {
    case mojom::FeaturePolicyFeature::kFontDisplay:
    case mojom::FeaturePolicyFeature::kUnoptimizedLosslessImages:
      return true;
    default:
      return false;
  }
}

void DocumentPolicy::UpdateFeatureState(const FeatureState& feature_state) {
  for (const auto& feature_and_value : feature_state) {
    // TODO(iclelland): Generate this switch block
    switch (feature_and_value.first) {
      case mojom::FeaturePolicyFeature::kFontDisplay:
        font_display_ = feature_and_value.second.BoolValue();
        break;
      case mojom::FeaturePolicyFeature::kUnoptimizedLosslessImages:
        unoptimized_lossless_images_ = feature_and_value.second.DoubleValue();
        break;
      default:
        NOTREACHED();
    }
  }
}

DocumentPolicy::FeatureState DocumentPolicy::GetFeatureState() const {
  FeatureState feature_state;
  // TODO(iclelland): Generate this block
  feature_state[mojom::FeaturePolicyFeature::kFontDisplay] =
      PolicyValue(font_display_);
  feature_state[mojom::FeaturePolicyFeature::kUnoptimizedLosslessImages] =
      PolicyValue(unoptimized_lossless_images_);
  return feature_state;
}

DocumentPolicy::DocumentPolicy(const FeatureState& defaults) {
  UpdateFeatureState(defaults);
}

DocumentPolicy::~DocumentPolicy() = default;

// static
std::unique_ptr<DocumentPolicy> DocumentPolicy::CreateWithRequiredPolicy(
    const FeatureState& required_policy,
    const DocumentPolicy::FeatureState& defaults) {
  std::unique_ptr<DocumentPolicy> new_policy =
      base::WrapUnique(new DocumentPolicy(defaults));
  new_policy->UpdateFeatureState(required_policy);
  return new_policy;
}

// static
// TODO(iclelland): This list just contains two sample features for use during
// development. It should be generated from definitions in a feature
// configuration json5 file.
const DocumentPolicy::FeatureState& DocumentPolicy::GetFeatureDefaults() {
  static base::NoDestructor<FeatureState> default_feature_list(
      {{mojom::FeaturePolicyFeature::kFontDisplay, PolicyValue(false)},
       {mojom::FeaturePolicyFeature::kUnoptimizedLosslessImages,
        PolicyValue(2.0f)}});
  return *default_feature_list;
}

bool DocumentPolicy::IsPolicyCompatible(
    const DocumentPolicy::FeatureState& incoming_policy) {
  const DocumentPolicy::FeatureState& state = GetFeatureState();
  for (const auto& e : incoming_policy) {
    // feature value > threshold => enabled
    // value_a > value_b => value_a less strict than value_b
    if (state.at(e.first) > e.second)
      return false;
  }
  return true;
}
}  // namespace blink
