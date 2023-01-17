// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/html_document_parser_fastpath.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"

namespace blink {
namespace {

TEST(HTMLDocumentParserFastpathTest, SanityCheck) {
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  document->write("<body></body>");
  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);
  document->body()->AppendChild(div);
  DocumentFragment* fragment = DocumentFragment::Create(*document);
  EXPECT_TRUE(TryParsingHTMLFragment(
      "<div>test</div>", *document, *fragment, *div,
      ParserContentPolicy::kAllowScriptingContent, false));
}

TEST(HTMLDocumentParserFastpathTest, SetInnerHTMLUsesFastPathSuccess) {
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  document->write("<body></body>");
  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);

  base::HistogramTester histogram_tester;
  div->setInnerHTML("<div>test</div>");
  // This was html the fast path handled, so there should be one histogram with
  // success.
  histogram_tester.ExpectTotalCount("Blink.HTMLFastPathParser.ParseResult", 1);
  histogram_tester.ExpectUniqueSample("Blink.HTMLFastPathParser.ParseResult",
                                      HtmlFastPathResult::kSucceeded, 1);
}

TEST(HTMLDocumentParserFastpathTest, SetInnerHTMLUsesFastPathFailure) {
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  document->write("<body></body>");
  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);

  base::HistogramTester histogram_tester;
  div->setInnerHTML("<div");
  // The fast path should not have handled this, so there should be one
  // histogram with a value other then success.
  histogram_tester.ExpectTotalCount("Blink.HTMLFastPathParser.ParseResult", 1);
  histogram_tester.ExpectBucketCount("Blink.HTMLFastPathParser.ParseResult",
                                     HtmlFastPathResult::kSucceeded, 0);
}

}  // namespace
}  // namespace blink
