// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/locale/special_locale_handler.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/util.h"
#include "jni/SpecialLocaleHandler_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;

class PrefService;
class TemplateURL;

namespace {

Profile* GetOriginalProfile() {
  return ProfileManager::GetActiveUserProfile()->GetOriginalProfile();
}

int GetDesignatedPrepopulatedIdForLocale(const std::string& locale) {
  // TODO: Implement this as a map from locale to prepopulated engines.
  return TemplateURLPrepopulateData::sogou.id;
}

}  // namespace

static jlong Init(JNIEnv* env,
                  const JavaParamRef<jclass>& clazz,
                  const JavaParamRef<jstring>& jlocale) {
  return reinterpret_cast<intptr_t>(
      new SpecialLocaleHandler(ConvertJavaStringToUTF8(env, jlocale)));
}

SpecialLocaleHandler::SpecialLocaleHandler(const std::string& locale)
    : locale_(locale),
      template_url_service_(
          TemplateURLServiceFactory::GetForProfile(GetOriginalProfile())) {}

void SpecialLocaleHandler::Destroy(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj) {
  delete this;
}

jboolean SpecialLocaleHandler::LoadTemplateUrls(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  DCHECK(locale_.length() == 2);

  std::vector<std::unique_ptr<TemplateURLData>> prepopulated_list =
      TemplateURLPrepopulateData::GetLocalPrepopulatedEngines(
          locale_, GetOriginalProfile()->GetPrefs());

  if (prepopulated_list.empty())
    return false;

  for (const auto& data_url : prepopulated_list) {
    data_url.get()->show_in_default_list = false;
    data_url.get()->safe_for_autoreplace = true;
    std::unique_ptr<TemplateURL> turl(
        new TemplateURL(*data_url, TemplateURL::LOCAL));
    TemplateURL* added_turl = template_url_service_->Add(std::move(turl));
    if (added_turl) {
      prepopulate_ids_.push_back(added_turl->prepopulate_id());
    }
  }
  return true;
}

void SpecialLocaleHandler::RemoveTemplateUrls(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  while (!prepopulate_ids_.empty()) {
    TemplateURL* turl = FindURLByPrepopulateID(
        template_url_service_->GetTemplateURLs(), prepopulate_ids_.back());
    if (turl && template_url_service_->GetDefaultSearchProvider() != turl) {
      template_url_service_->Remove(turl);
    }
    prepopulate_ids_.pop_back();
  }
}

void SpecialLocaleHandler::OverrideDefaultSearchProvider(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  // If the user has changed their default search provider, no-op.
  TemplateURL* current_dsp = template_url_service_->GetDefaultSearchProvider();
  if (!current_dsp ||
      current_dsp->prepopulate_id() != TemplateURLPrepopulateData::google.id) {
    return;
  }

  TemplateURL* turl =
      FindURLByPrepopulateID(template_url_service_->GetTemplateURLs(),
                             GetDesignatedPrepopulatedIdForLocale(locale_));
  if (turl) {
    template_url_service_->SetUserSelectedDefaultSearchProvider(turl);
  }
}

void SpecialLocaleHandler::SetGoogleAsDefaultSearch(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  // If the user has changed their default search provider, no-op.
  TemplateURL* current_dsp = template_url_service_->GetDefaultSearchProvider();
  if (!current_dsp ||
      current_dsp->prepopulate_id() !=
          GetDesignatedPrepopulatedIdForLocale(locale_)) {
    return;
  }

  TemplateURL* turl =
      FindURLByPrepopulateID(template_url_service_->GetTemplateURLs(),
                             TemplateURLPrepopulateData::google.id);
  if (turl) {
    template_url_service_->SetUserSelectedDefaultSearchProvider(turl);
  }
}

SpecialLocaleHandler::~SpecialLocaleHandler() {}

// static
bool RegisterSpecialLocaleHandler(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
