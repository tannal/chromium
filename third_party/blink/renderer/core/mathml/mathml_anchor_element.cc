// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mathml/mathml_anchor_element.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/html/anchor_element_utils.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/loader/navigation_policy.h"

#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/layout/mathml/layout_mathml_block.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

bool IsEnterKeyKeydownEvent(Event& event);

bool IsLinkClick(Event& event);

MathMLAnchorElement::MathMLAnchorElement(Document& document)
    : MathMLElement(mathml_names::kATag, document),
      rel_list_(MakeGarbageCollected<RelList>(this, html_names::kRelAttr)),
      link_relations_(0) {}

void MathMLAnchorElement::Trace(Visitor* visitor) const {
  visitor->Trace(rel_list_);
  MathMLElement::Trace(visitor);
}

LayoutObject* MathMLAnchorElement::CreateLayoutObject(
    const ComputedStyle& style) {
  return MathMLElement::CreateLayoutObject(style);
}

void MathMLAnchorElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kHrefAttr) {
    bool was_link = IsLink();
    SetIsLink(!params.new_value.IsNull());
    if (was_link != IsLink()) {
      PseudoStateChanged(CSSSelector::kPseudoLink);
      PseudoStateChanged(CSSSelector::kPseudoVisited);
      PseudoStateChanged(CSSSelector::kPseudoAnyLink);
    }
  } else if (params.name == html_names::kRelAttr) {
    link_relations_ =
        AnchorElementUtils::ParseRelAttribute(params.new_value, GetDocument());
    rel_list_->DidUpdateAttributeValue(params.old_value, params.new_value);
  } else {
    MathMLElement::ParseAttribute(params);
  }
}

bool MathMLAnchorElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == html_names::kHrefAttr ||
         MathMLElement::IsURLAttribute(attribute);
}

KURL MathMLAnchorElement::Url() const {
  return GetDocument().CompleteURL(
      StripLeadingAndTrailingHtmlSpaces(FastGetAttribute(html_names::kHrefAttr)));
}

void MathMLAnchorElement::SetURL(const KURL& url) {
  setAttribute(html_names::kHrefAttr, AtomicString(url.GetString()));
}

String MathMLAnchorElement::Input() const {
  return Url();
}

bool MathMLAnchorElement::HasActivationBehavior() const {
  return IsLink();
}

void MathMLAnchorElement::DefaultEventHandler(Event& event) {
  if (IsLink()) {
    if (IsFocused() && IsEnterKeyKeydownEvent(event)) {
      event.SetDefaultHandled();
      DispatchSimulatedClick(&event);
      return;
    }

    if (IsLinkClick(event)) {
      HandleClick(To<MouseEvent>(event));
      return;
    }
  }
  MathMLElement::DefaultEventHandler(event);
}

void MathMLAnchorElement::HandleClick(MouseEvent& event) {
  event.SetDefaultHandled();

  LocalDOMWindow* window = GetDocument().domWindow();
  if (!window || !window->GetFrame()) {
    return;
  }

  const KURL& completed_url = GetDocument().CompleteURL(
      StripLeadingAndTrailingHtmlSpaces(FastGetAttribute(html_names::kHrefAttr)));

  AnchorElementUtils::SendPings(
      completed_url, GetDocument(), FastGetAttribute(html_names::kPingAttr));

  ResourceRequest request(completed_url);
  AnchorElementUtils::HandleReferrerPolicyAttribute(
      request, FastGetAttribute(html_names::kReferrerpolicyAttr),
      link_relations_, GetDocument());

  if (AnchorElementUtils::HasRel(link_relations_, kRelationNoReferrer)) {
    request.SetReferrerPolicy(network::mojom::ReferrerPolicy::kNever);
  }

  request.SetHasUserGesture(
      LocalFrame::HasTransientUserActivation(window->GetFrame()));
  NavigationPolicy navigation_policy = NavigationPolicyFromEvent(&event);

  if (FastHasAttribute(html_names::kDownloadAttr) &&
      navigation_policy != kNavigationPolicyDownload &&
      window->GetSecurityOrigin()->CanReadContent(completed_url)) {
    const String download_attr =
        FastGetAttribute(html_names::kDownloadAttr);
    AnchorElementUtils::HandleDownloadAttribute(
        this, download_attr, completed_url, window, event.isTrusted(),
        std::move(request));
    return;
  }

  FrameLoadRequest frame_request(window, request);
  frame_request.SetNavigationPolicy(navigation_policy);
  frame_request.SetClientNavigationReason(ClientNavigationReason::kAnchorClick);
  frame_request.SetSourceElement(this);

  if (AnchorElementUtils::HasRel(link_relations_, kRelationNoOpener)) {
    frame_request.SetNoOpener();
  }

  AtomicString target(FastGetAttribute(html_names::kTargetAttr));
  frame_request.SetTriggeringEventInfo(
      event.isTrusted()
          ? mojom::blink::TriggeringEventInfo::kFromTrustedEvent
          : mojom::blink::TriggeringEventInfo::kFromUntrustedEvent);

  if (Frame* target_frame =
          window->GetFrame()
              ->Tree()
              .FindOrCreateFrameForNavigation(frame_request, target)
              .frame) {
    target_frame->Navigate(frame_request, WebFrameLoadType::kStandard);
  }
}

FocusableState MathMLAnchorElement::SupportsFocus(
    UpdateBehavior update_behavior) const {
  if (IsLink()) {
    return FocusableState::kFocusable;
  }
  return MathMLElement::SupportsFocus(update_behavior);
}

int MathMLAnchorElement::DefaultTabIndex() const {
  return 0;
}

}  // namespace blink
