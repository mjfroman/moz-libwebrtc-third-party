// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/web_bundle/script_web_bundle_rule.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(ScriptWebBundleRuleTest, Empty) {
  EXPECT_FALSE(
      ScriptWebBundleRule::ParseJson("", KURL("https://example.com/")));
}

TEST(ScriptWebBundleRuleTest, Basic) {
  const KURL base_url("https://example.com/");
  auto result = ScriptWebBundleRule::ParseJson(
      R"({
        "source": "foo.wbn",
        "scopes": ["js"],
        "resources": ["dir/a.css", "dir/b.css"]
      })",
      base_url);
  ASSERT_TRUE(result);
  ScriptWebBundleRule& rule = *result;
  EXPECT_EQ(rule.source_url(), "https://example.com/foo.wbn");
  EXPECT_THAT(rule.scope_urls(),
              testing::UnorderedElementsAre("https://example.com/js"));
  EXPECT_THAT(rule.resource_urls(),
              testing::UnorderedElementsAre("https://example.com/dir/a.css",
                                            "https://example.com/dir/b.css"));
}

TEST(ScriptWebBundleRuleTest, SourceOnly) {
  const KURL base_url("https://example.com/");
  auto result = ScriptWebBundleRule::ParseJson(
      R"({
        "source": "foo.wbn"
      })",
      base_url);
  ASSERT_TRUE(result);
  ScriptWebBundleRule& rule = *result;
  EXPECT_EQ(rule.source_url(), "https://example.com/foo.wbn");
  EXPECT_TRUE(rule.scope_urls().IsEmpty());
  EXPECT_TRUE(rule.resource_urls().IsEmpty());
}

TEST(ScriptWebBundleRuleTest, InvalidType) {
  const KURL base_url("https://example.com/");
  // `scopes` and `resources` should be JSON array.
  auto result = ScriptWebBundleRule::ParseJson(
      R"({
        "source": "foo.wbn",
        "scopes": "js",
        "resources":  { "a": "hello" }
      })",
      base_url);
  ASSERT_TRUE(result);
  ScriptWebBundleRule& rule = *result;
  EXPECT_EQ(rule.source_url(), "https://example.com/foo.wbn");
  EXPECT_TRUE(rule.scope_urls().IsEmpty());
  EXPECT_TRUE(rule.resource_urls().IsEmpty());
}

// TODO(crbug.com/1245166): Add more tests.

}  // namespace blink
