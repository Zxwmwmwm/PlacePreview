#include "placepreview/ColorPreviewOverlay.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <string>

#if defined(__ANDROID__)
#include <dlfcn.h>
#include <jni.h>
#endif

namespace placepreview::ui {

#if defined(__ANDROID__)

namespace {

std::atomic<JavaVM*> gJavaVm{nullptr};
std::mutex gOverlayMutex;

struct OverlayWidgets {
    jobject activityRef{nullptr};
    jobject dialogRef{nullptr};
    jobject cardViewRef{nullptr};
    jobject pureSwatchDrawableRef{nullptr};
    jobject simSwatchDrawableRef{nullptr};
    jobject labelViewRef{nullptr};
    float density{2.75f};
};

OverlayWidgets gOverlay{};

bool clearJniException(JNIEnv* env) noexcept {
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return true;
    }
    return false;
}

JavaVM* resolveJavaVm() noexcept {
    JavaVM* vm = gJavaVm.load(std::memory_order_acquire);
    if (vm != nullptr) {
        return vm;
    }
    using FnGetJavaVm = JavaVM* (*)();
    void* sym = dlsym(RTLD_DEFAULT, "_ZN2pl7runtime9GetJavaVmEv");
    if (sym != nullptr) {
        auto fn = reinterpret_cast<FnGetJavaVm>(sym);
        vm = fn();
        if (vm != nullptr) {
            gJavaVm.store(vm, std::memory_order_release);
        }
    }
    return vm;
}

JNIEnv* getAttachedUiThreadEnv() noexcept {
    JavaVM* vm = resolveJavaVm();
    if (!vm) return nullptr;
    void* rawEnv = nullptr;
    if (vm->GetEnv(&rawEnv, JNI_VERSION_1_6) != JNI_OK || !rawEnv) {
        return nullptr;
    }
    return static_cast<JNIEnv*>(rawEnv);
}

int dpPx(float density, float dp) noexcept {
    return std::max(1, static_cast<int>(std::lround(dp * density)));
}

jobject findCurrentResumedActivity(JNIEnv* env) noexcept {
    jclass atClass = env->FindClass("android/app/ActivityThread");
    if (clearJniException(env) || !atClass) return nullptr;

    jmethodID currentAt = env->GetStaticMethodID(
        atClass, "currentActivityThread", "()Landroid/app/ActivityThread;");
    if (clearJniException(env) || !currentAt) {
        env->DeleteLocalRef(atClass);
        return nullptr;
    }

    jobject atObj = env->CallStaticObjectMethod(atClass, currentAt);
    if (clearJniException(env) || !atObj) {
        env->DeleteLocalRef(atClass);
        return nullptr;
    }

    jfieldID fActivities = env->GetFieldID(atClass, "mActivities", "Landroid/util/ArrayMap;");
    if (clearJniException(env) || !fActivities) {
        fActivities = env->GetFieldID(atClass, "mActivities", "Ljava/util/Map;");
        if (clearJniException(env) || !fActivities) {
            env->DeleteLocalRef(atObj);
            env->DeleteLocalRef(atClass);
            return nullptr;
        }
    }

    jobject mapObj = env->GetObjectField(atObj, fActivities);
    env->DeleteLocalRef(atObj);
    env->DeleteLocalRef(atClass);
    if (clearJniException(env) || !mapObj) return nullptr;

    jclass mapClass = env->FindClass("java/util/Map");
    jmethodID mValues = mapClass ? env->GetMethodID(mapClass, "values", "()Ljava/util/Collection;") : nullptr;
    if (clearJniException(env) || !mValues) {
        if (mapClass) env->DeleteLocalRef(mapClass);
        env->DeleteLocalRef(mapObj);
        return nullptr;
    }

    jobject colObj = env->CallObjectMethod(mapObj, mValues);
    env->DeleteLocalRef(mapClass);
    env->DeleteLocalRef(mapObj);
    if (clearJniException(env) || !colObj) return nullptr;

    jclass colClass = env->FindClass("java/util/Collection");
    jmethodID mIter = colClass ? env->GetMethodID(colClass, "iterator", "()Ljava/util/Iterator;") : nullptr;
    if (clearJniException(env) || !mIter) {
        if (colClass) env->DeleteLocalRef(colClass);
        env->DeleteLocalRef(colObj);
        return nullptr;
    }

    jobject iterObj = env->CallObjectMethod(colObj, mIter);
    env->DeleteLocalRef(colClass);
    env->DeleteLocalRef(colObj);
    if (clearJniException(env) || !iterObj) return nullptr;

    jclass iterClass = env->FindClass("java/util/Iterator");
    jmethodID mHasNext = iterClass ? env->GetMethodID(iterClass, "hasNext", "()Z") : nullptr;
    jmethodID mNext = iterClass ? env->GetMethodID(iterClass, "next", "()Ljava/lang/Object;") : nullptr;
    if (clearJniException(env) || !mHasNext || !mNext) {
        if (iterClass) env->DeleteLocalRef(iterClass);
        env->DeleteLocalRef(iterObj);
        return nullptr;
    }

    jobject chosenActivity = nullptr;
    jobject fallbackActivity = nullptr;

    while (env->CallBooleanMethod(iterObj, mHasNext)) {
        if (clearJniException(env)) break;
        jobject rec = env->CallObjectMethod(iterObj, mNext);
        if (clearJniException(env) || !rec) continue;

        jclass recClass = env->GetObjectClass(rec);
        jfieldID fPaused = recClass ? env->GetFieldID(recClass, "paused", "Z") : nullptr;
        jfieldID fActivity = recClass ? env->GetFieldID(recClass, "activity", "Landroid/app/Activity;") : nullptr;
        if (clearJniException(env) || !fPaused || !fActivity) {
            if (recClass) env->DeleteLocalRef(recClass);
            env->DeleteLocalRef(rec);
            continue;
        }

        const jboolean paused = env->GetBooleanField(rec, fPaused);
        jobject act = env->GetObjectField(rec, fActivity);
        env->DeleteLocalRef(recClass);
        env->DeleteLocalRef(rec);
        if (clearJniException(env) || !act) continue;

        if (!paused) {
            chosenActivity = act;
            break;
        }
        if (!fallbackActivity) {
            fallbackActivity = act;
        } else {
            env->DeleteLocalRef(act);
        }
    }

    env->DeleteLocalRef(iterClass);
    env->DeleteLocalRef(iterObj);

    if (chosenActivity) {
        if (fallbackActivity) env->DeleteLocalRef(fallbackActivity);
        return chosenActivity;
    }
    return fallbackActivity;
}

float readDisplayDensity(JNIEnv* env, jobject activity) noexcept {
    jclass ctxClass = env->FindClass("android/content/Context");
    jmethodID mGetRes = ctxClass ? env->GetMethodID(ctxClass, "getResources", "()Landroid/content/res/Resources;") : nullptr;
    if (clearJniException(env) || !mGetRes) {
        if (ctxClass) env->DeleteLocalRef(ctxClass);
        return 2.75f;
    }
    jobject resObj = env->CallObjectMethod(activity, mGetRes);
    env->DeleteLocalRef(ctxClass);
    if (clearJniException(env) || !resObj) return 2.75f;

    jclass resClass = env->FindClass("android/content/res/Resources");
    jmethodID mGetDm = resClass ? env->GetMethodID(resClass, "getDisplayMetrics", "()Landroid/util/DisplayMetrics;") : nullptr;
    if (clearJniException(env) || !mGetDm) {
        if (resClass) env->DeleteLocalRef(resClass);
        env->DeleteLocalRef(resObj);
        return 2.75f;
    }
    jobject dmObj = env->CallObjectMethod(resObj, mGetDm);
    env->DeleteLocalRef(resClass);
    env->DeleteLocalRef(resObj);
    if (clearJniException(env) || !dmObj) return 2.75f;

    jclass dmClass = env->FindClass("android/util/DisplayMetrics");
    jfieldID fDensity = dmClass ? env->GetFieldID(dmClass, "density", "F") : nullptr;
    float density = 2.75f;
    if (!clearJniException(env) && fDensity) {
        const jfloat val = env->GetFloatField(dmObj, fDensity);
        if (!clearJniException(env) && val > 0.5f && val < 8.0f) {
            density = static_cast<float>(val);
        }
    }
    if (dmClass) env->DeleteLocalRef(dmClass);
    env->DeleteLocalRef(dmObj);
    return density;
}

void releaseOverlayRefsLocked(JNIEnv* env, bool callDismiss) noexcept {
    if (env && callDismiss && gOverlay.dialogRef) {
        jclass dlgClass = env->FindClass("android/app/Dialog");
        if (!clearJniException(env) && dlgClass) {
            jmethodID mDismiss = env->GetMethodID(dlgClass, "dismiss", "()V");
            if (!clearJniException(env) && mDismiss) {
                env->CallVoidMethod(gOverlay.dialogRef, mDismiss);
                (void)clearJniException(env);
            }
            env->DeleteLocalRef(dlgClass);
        }
    }
    if (env) {
        if (gOverlay.labelViewRef) env->DeleteGlobalRef(gOverlay.labelViewRef);
        if (gOverlay.simSwatchDrawableRef) env->DeleteGlobalRef(gOverlay.simSwatchDrawableRef);
        if (gOverlay.pureSwatchDrawableRef) env->DeleteGlobalRef(gOverlay.pureSwatchDrawableRef);
        if (gOverlay.cardViewRef) env->DeleteGlobalRef(gOverlay.cardViewRef);
        if (gOverlay.dialogRef) env->DeleteGlobalRef(gOverlay.dialogRef);
        if (gOverlay.activityRef) env->DeleteGlobalRef(gOverlay.activityRef);
    }
    gOverlay = OverlayWidgets{};
}

jobject makeRoundedDrawable(JNIEnv* env, float radiusPx, jint fillArgb, int strokeWidthPx, jint strokeArgb) noexcept {
    jclass gdClass = env->FindClass("android/graphics/drawable/GradientDrawable");
    if (clearJniException(env) || !gdClass) return nullptr;
    jmethodID ctor = env->GetMethodID(gdClass, "<init>", "()V");
    jmethodID setColor = env->GetMethodID(gdClass, "setColor", "(I)V");
    jmethodID setCorner = env->GetMethodID(gdClass, "setCornerRadius", "(F)V");
    jmethodID setStroke = env->GetMethodID(gdClass, "setStroke", "(II)V");
    if (clearJniException(env) || !ctor || !setColor || !setCorner || !setStroke) {
        env->DeleteLocalRef(gdClass);
        return nullptr;
    }

    jobject gd = env->NewObject(gdClass, ctor);
    if (clearJniException(env) || !gd) {
        env->DeleteLocalRef(gdClass);
        return nullptr;
    }
    env->CallVoidMethod(gd, setCorner, static_cast<jfloat>(radiusPx));
    env->CallVoidMethod(gd, setColor, fillArgb);
    env->CallVoidMethod(gd, setStroke, static_cast<jint>(strokeWidthPx), strokeArgb);
    (void)clearJniException(env);
    env->DeleteLocalRef(gdClass);
    return gd;
}

bool ensureOverlayCreatedLocked(JNIEnv* env, jobject activity) noexcept {
    if (gOverlay.dialogRef && gOverlay.activityRef &&
        env->IsSameObject(gOverlay.activityRef, activity)) {
        return true;
    }

    releaseOverlayRefsLocked(env, true);

    const float d = readDisplayDensity(env, activity);

    jclass dlgClass = env->FindClass("android/app/Dialog");
    jclass winClass = env->FindClass("android/view/Window");
    jclass lpClass = env->FindClass("android/view/WindowManager$LayoutParams");
    jclass cdClass = env->FindClass("android/graphics/drawable/ColorDrawable");
    jclass llClass = env->FindClass("android/widget/LinearLayout");
    jclass llLpClass = env->FindClass("android/widget/LinearLayout$LayoutParams");
    jclass viewClass = env->FindClass("android/view/View");
    jclass tvClass = env->FindClass("android/widget/TextView");
    if (clearJniException(env) || !dlgClass || !winClass || !lpClass || !cdClass ||
        !llClass || !llLpClass || !viewClass || !tvClass) {
        return false;
    }

    jmethodID dlgCtor = env->GetMethodID(dlgClass, "<init>", "(Landroid/content/Context;)V");
    jmethodID reqFeat = env->GetMethodID(dlgClass, "requestWindowFeature", "(I)Z");
    jmethodID setCancel = env->GetMethodID(dlgClass, "setCancelable", "(Z)V");
    jmethodID setContent = env->GetMethodID(dlgClass, "setContentView", "(Landroid/view/View;)V");
    jmethodID getWindow = env->GetMethodID(dlgClass, "getWindow", "()Landroid/view/Window;");

    jmethodID cdCtor = env->GetMethodID(cdClass, "<init>", "(I)V");
    jmethodID setBgDraw = env->GetMethodID(winClass, "setBackgroundDrawable", "(Landroid/graphics/drawable/Drawable;)V");
    jmethodID clearFlags = env->GetMethodID(winClass, "clearFlags", "(I)V");
    jmethodID addFlags = env->GetMethodID(winClass, "addFlags", "(I)V");
    jmethodID getAttrs = env->GetMethodID(winClass, "getAttributes", "()Landroid/view/WindowManager$LayoutParams;");
    jmethodID setAttrs = env->GetMethodID(winClass, "setAttributes", "(Landroid/view/WindowManager$LayoutParams;)V");

    jfieldID fWidth = env->GetFieldID(lpClass, "width", "I");
    jfieldID fHeight = env->GetFieldID(lpClass, "height", "I");
    jfieldID fGravity = env->GetFieldID(lpClass, "gravity", "I");
    jfieldID fY = env->GetFieldID(lpClass, "y", "I");
    jfieldID fDim = env->GetFieldID(lpClass, "dimAmount", "F");

    jmethodID llCtor = env->GetMethodID(llClass, "<init>", "(Landroid/content/Context;)V");
    jmethodID llSetOrient = env->GetMethodID(llClass, "setOrientation", "(I)V");
    jmethodID llSetGrav = env->GetMethodID(llClass, "setGravity", "(I)V");
    jmethodID viewSetPad = env->GetMethodID(viewClass, "setPadding", "(IIII)V");
    jmethodID viewSetBg = env->GetMethodID(viewClass, "setBackground", "(Landroid/graphics/drawable/Drawable;)V");
    jmethodID vgAddView = env->GetMethodID(llClass, "addView", "(Landroid/view/View;Landroid/view/ViewGroup$LayoutParams;)V");
    jmethodID viewCtor = env->GetMethodID(viewClass, "<init>", "(Landroid/content/Context;)V");
    jmethodID llLpCtor = env->GetMethodID(llLpClass, "<init>", "(II)V");
    jmethodID llLpMargins = env->GetMethodID(llLpClass, "setMargins", "(IIII)V");

    jmethodID tvCtor = env->GetMethodID(tvClass, "<init>", "(Landroid/content/Context;)V");
    jmethodID tvSetColor = env->GetMethodID(tvClass, "setTextColor", "(I)V");
    jmethodID tvSetSize = env->GetMethodID(tvClass, "setTextSize", "(IF)V");
    jmethodID tvSingle = env->GetMethodID(tvClass, "setSingleLine", "(Z)V");

    if (clearJniException(env) || !dlgCtor || !reqFeat || !setCancel || !setContent || !getWindow ||
        !cdCtor || !setBgDraw || !clearFlags || !addFlags || !getAttrs || !setAttrs ||
        !fWidth || !fHeight || !fGravity || !fY || !fDim ||
        !llCtor || !llSetOrient || !llSetGrav || !viewSetPad || !viewSetBg || !vgAddView ||
        !viewCtor || !llLpCtor || !llLpMargins || !tvCtor || !tvSetColor || !tvSetSize || !tvSingle) {
        return false;
    }

    jobject dlg = env->NewObject(dlgClass, dlgCtor, activity);
    if (clearJniException(env) || !dlg) return false;

    (void)env->CallBooleanMethod(dlg, reqFeat, 1); // Window.FEATURE_NO_TITLE = 1
    env->CallVoidMethod(dlg, setCancel, JNI_FALSE);
    (void)clearJniException(env);

    // Outer card container
    jobject card = env->NewObject(llClass, llCtor, activity);
    env->CallVoidMethod(card, llSetOrient, 0); // HORIZONTAL
    env->CallVoidMethod(card, llSetGrav, 16);  // CENTER_VERTICAL
    env->CallVoidMethod(card, viewSetPad, dpPx(d, 12.0f), dpPx(d, 7.0f), dpPx(d, 14.0f), dpPx(d, 7.0f));
    jobject cardBg = makeRoundedDrawable(
        env, d * 12.0f, static_cast<jint>(0xF21B1E26u), dpPx(d, 1.5f), static_cast<jint>(0x55FFFFFFu));
    if (cardBg) {
        env->CallVoidMethod(card, viewSetBg, cardBg);
        env->DeleteLocalRef(cardBg);
    }

    // Swatch 1: pure opaque RGB hue box (always clear even at 0% fill opacity)
    jobject pureView = env->NewObject(viewClass, viewCtor, activity);
    jobject pureDraw = makeRoundedDrawable(
        env, d * 6.0f, static_cast<jint>(0xFFFFFFFFu), dpPx(d, 1.0f), static_cast<jint>(0x88FFFFFFu));
    if (pureDraw) {
        env->CallVoidMethod(pureView, viewSetBg, pureDraw);
    }
    jobject lpPure = env->NewObject(llLpClass, llLpCtor, dpPx(d, 22.0f), dpPx(d, 22.0f));
    env->CallVoidMethod(lpPure, llLpMargins, 0, 0, dpPx(d, 7.0f), 0);
    env->CallVoidMethod(card, vgAddView, pureView, lpPure);

    // Swatch 2: live simulated Fill Opacity + Outline Opacity box over dark backing
    jobject simBacking = env->NewObject(llClass, llCtor, activity);
    jobject simBackBg = makeRoundedDrawable(
        env, d * 6.0f, static_cast<jint>(0xFF101218u), 0, 0);
    if (simBackBg) {
        env->CallVoidMethod(simBacking, viewSetBg, simBackBg);
        env->DeleteLocalRef(simBackBg);
    }
    jobject simView = env->NewObject(viewClass, viewCtor, activity);
    jobject simDraw = makeRoundedDrawable(
        env, d * 6.0f, static_cast<jint>(0x57FFFFFFu), dpPx(d, 2.0f), static_cast<jint>(0xB8FFFFFFu));
    if (simDraw) {
        env->CallVoidMethod(simView, viewSetBg, simDraw);
    }
    jobject lpSimInner = env->NewObject(llLpClass, llLpCtor, dpPx(d, 26.0f), dpPx(d, 22.0f));
    env->CallVoidMethod(simBacking, vgAddView, simView, lpSimInner);
    jobject lpSimOuter = env->NewObject(llLpClass, llLpCtor, dpPx(d, 26.0f), dpPx(d, 22.0f));
    env->CallVoidMethod(lpSimOuter, llLpMargins, 0, 0, dpPx(d, 10.0f), 0);
    env->CallVoidMethod(card, vgAddView, simBacking, lpSimOuter);

    // Live text label (#RRGGBB · RGB · Fill & Outline %)
    jobject label = env->NewObject(tvClass, tvCtor, activity);
    env->CallVoidMethod(label, tvSetColor, static_cast<jint>(0xFFF4F6FAu));
    env->CallVoidMethod(label, tvSetSize, 2, 12.5f); // TypedValue.COMPLEX_UNIT_SP = 2
    env->CallVoidMethod(label, tvSingle, JNI_TRUE);
    jobject lpText = env->NewObject(llLpClass, llLpCtor, -2, -2); // WRAP_CONTENT
    env->CallVoidMethod(card, vgAddView, label, lpText);

    env->CallVoidMethod(dlg, setContent, card);

    jobject win = env->CallObjectMethod(dlg, getWindow);
    if (!clearJniException(env) && win) {
        jobject trans = env->NewObject(cdClass, cdCtor, 0);
        if (trans) {
            env->CallVoidMethod(win, setBgDraw, trans);
            env->DeleteLocalRef(trans);
        }
        // FLAG_DIM_BEHIND = 0x2; FLAG_NOT_FOCUSABLE = 0x8; FLAG_NOT_TOUCHABLE = 0x10; FLAG_LAYOUT_IN_SCREEN = 0x100
        env->CallVoidMethod(win, clearFlags, 0x00000002);
        env->CallVoidMethod(win, addFlags, 0x00000008 | 0x00000010 | 0x00000100);
        jobject attrs = env->CallObjectMethod(win, getAttrs);
        if (!clearJniException(env) && attrs) {
            env->SetIntField(attrs, fWidth, -2);   // WRAP_CONTENT
            env->SetIntField(attrs, fHeight, -2);  // WRAP_CONTENT
            env->SetIntField(attrs, fGravity, 49); // Gravity.TOP (48) | Gravity.CENTER_HORIZONTAL (1)
            env->SetIntField(attrs, fY, dpPx(d, 6.0f));
            env->SetFloatField(attrs, fDim, 0.0f);
            env->CallVoidMethod(win, setAttrs, attrs);
            env->DeleteLocalRef(attrs);
        }
        env->DeleteLocalRef(win);
    }
    (void)clearJniException(env);

    gOverlay.activityRef = env->NewGlobalRef(activity);
    gOverlay.dialogRef = env->NewGlobalRef(dlg);
    gOverlay.cardViewRef = env->NewGlobalRef(card);
    gOverlay.pureSwatchDrawableRef = pureDraw ? env->NewGlobalRef(pureDraw) : nullptr;
    gOverlay.simSwatchDrawableRef = simDraw ? env->NewGlobalRef(simDraw) : nullptr;
    gOverlay.labelViewRef = env->NewGlobalRef(label);
    gOverlay.density = d;

    env->DeleteLocalRef(lpText);
    env->DeleteLocalRef(label);
    env->DeleteLocalRef(lpSimOuter);
    env->DeleteLocalRef(lpSimInner);
    env->DeleteLocalRef(simView);
    env->DeleteLocalRef(simBacking);
    if (simDraw) env->DeleteLocalRef(simDraw);
    env->DeleteLocalRef(lpPure);
    env->DeleteLocalRef(pureView);
    if (pureDraw) env->DeleteLocalRef(pureDraw);
    env->DeleteLocalRef(card);
    env->DeleteLocalRef(dlg);
    return gOverlay.dialogRef != nullptr;
}

} // namespace

#endif // defined(__ANDROID__)

void showLiveColorPreview(const ModConfig& cfg) noexcept {
#if defined(__ANDROID__)
    JNIEnv* env = getAttachedUiThreadEnv();
    if (!env) return;

    if (env->PushLocalFrame(48) < 0) {
        (void)clearJniException(env);
        return;
    }

    std::lock_guard<std::mutex> lock(gOverlayMutex);
    jobject activity = findCurrentResumedActivity(env);
    if (!activity || !ensureOverlayCreatedLocked(env, activity)) {
        env->PopLocalFrame(nullptr);
        return;
    }

    const uint32_t r = static_cast<uint32_t>(std::clamp(cfg.accentRed, 0, 255));
    const uint32_t g = static_cast<uint32_t>(std::clamp(cfg.accentGreen, 0, 255));
    const uint32_t b = static_cast<uint32_t>(std::clamp(cfg.accentBlue, 0, 255));
    const uint32_t rgb = (r << 16u) | (g << 8u) | b;

    const float fillEff = (cfg.renderMode == "Wireframe Only")
                              ? 0.0f
                              : std::clamp(cfg.fillOpacity * cfg.colorAlphaMultiplier(), 0.0f, 1.0f);
    const float lineEff = (cfg.renderMode == "Translucent Fill")
                              ? 0.0f
                              : std::clamp(cfg.wireframeOpacity * cfg.colorAlphaMultiplier(), 0.0f, 1.0f);
    const uint32_t fillA = static_cast<uint32_t>(std::lround(fillEff * 255.0f));
    const uint32_t lineA = static_cast<uint32_t>(std::lround(lineEff * 255.0f));

    jclass gdClass = env->FindClass("android/graphics/drawable/GradientDrawable");
    if (!clearJniException(env) && gdClass) {
        jmethodID setColor = env->GetMethodID(gdClass, "setColor", "(I)V");
        jmethodID setStroke = env->GetMethodID(gdClass, "setStroke", "(II)V");
        if (!clearJniException(env) && setColor && setStroke) {
            if (gOverlay.pureSwatchDrawableRef) {
                env->CallVoidMethod(
                    gOverlay.pureSwatchDrawableRef,
                    setColor,
                    static_cast<jint>(0xFF000000u | rgb));
            }
            if (gOverlay.simSwatchDrawableRef) {
                env->CallVoidMethod(
                    gOverlay.simSwatchDrawableRef,
                    setColor,
                    static_cast<jint>((fillA << 24u) | rgb));
                env->CallVoidMethod(
                    gOverlay.simSwatchDrawableRef,
                    setStroke,
                    static_cast<jint>(dpPx(gOverlay.density, 2.0f)),
                    static_cast<jint>((lineA << 24u) | rgb));
            }
            (void)clearJniException(env);
        }
    }

    char summary[128]{};
    const int fillPct = static_cast<int>(std::lround(fillEff * 100.0f));
    const int linePct = static_cast<int>(std::lround(lineEff * 100.0f));
    if (cfg.colorMode == "Material Adaptive") {
        std::snprintf(
            summary,
            sizeof(summary),
            "Material Adaptive  \xC2\xB7  Fill %d%%  \xC2\xB7  Outline %d%%",
            fillPct,
            linePct);
    } else {
        std::snprintf(
            summary,
            sizeof(summary),
            "#%02X%02X%02X  (%u, %u, %u)  \xC2\xB7  Fill %d%%  Outline %d%%",
            r,
            g,
            b,
            r,
            g,
            b,
            fillPct,
            linePct);
    }

    jclass tvClass = env->FindClass("android/widget/TextView");
    if (!clearJniException(env) && tvClass && gOverlay.labelViewRef) {
        jmethodID setText = env->GetMethodID(tvClass, "setText", "(Ljava/lang/CharSequence;)V");
        jstring jTxt = env->NewStringUTF(summary);
        if (!clearJniException(env) && setText && jTxt) {
            env->CallVoidMethod(gOverlay.labelViewRef, setText, jTxt);
            (void)clearJniException(env);
        }
    }

    jclass dlgClass = env->FindClass("android/app/Dialog");
    jclass viewClass = env->FindClass("android/view/View");
    jclass vpaClass = env->FindClass("android/view/ViewPropertyAnimator");
    if (!clearJniException(env) && dlgClass && viewClass && gOverlay.dialogRef && gOverlay.cardViewRef) {
        jmethodID getAlpha = env->GetMethodID(viewClass, "getAlpha", "()F");
        jmethodID isShowing = env->GetMethodID(dlgClass, "isShowing", "()Z");
        jmethodID hide = env->GetMethodID(dlgClass, "hide", "()V");
        jmethodID show = env->GetMethodID(dlgClass, "show", "()V");
        if (!clearJniException(env) && getAlpha && isShowing && hide && show) {
            const auto nowTp = std::chrono::steady_clock::now();
            static std::chrono::steady_clock::time_point sLastTouch{};
            const auto gapMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                nowTp - sLastTouch).count();
            sLastTouch = nowTp;

            const jfloat curAlpha = env->CallFloatMethod(gOverlay.cardViewRef, getAlpha);
            const jboolean showing = env->CallBooleanMethod(gOverlay.dialogRef, isShowing);
            (void)clearJniException(env);
            const bool needRaiseToFront = (curAlpha <= 0.05f) || (gapMs >= 450);
            // Re-raise above any newly opened ModMenu Dialog OR inner "Preview Color" AlertDialog!
            if (showing && needRaiseToFront) {
                env->CallVoidMethod(gOverlay.dialogRef, hide);
                (void)clearJniException(env);
            }
            if (!showing || needRaiseToFront) {
                env->CallVoidMethod(gOverlay.dialogRef, show);
                (void)clearJniException(env);
            }
        }
    }

    // Smooth hardware-accelerated auto-fade 3.2s after the user finishes dragging:
    if (!clearJniException(env) && viewClass && vpaClass && gOverlay.cardViewRef) {
        jmethodID setAlpha = env->GetMethodID(viewClass, "setAlpha", "(F)V");
        jmethodID animate = env->GetMethodID(viewClass, "animate", "()Landroid/view/ViewPropertyAnimator;");
        jmethodID cancel = env->GetMethodID(vpaClass, "cancel", "()V");
        jmethodID vpaAlpha = env->GetMethodID(vpaClass, "alpha", "(F)Landroid/view/ViewPropertyAnimator;");
        jmethodID vpaDelay = env->GetMethodID(vpaClass, "setStartDelay", "(J)Landroid/view/ViewPropertyAnimator;");
        jmethodID vpaDur = env->GetMethodID(vpaClass, "setDuration", "(J)Landroid/view/ViewPropertyAnimator;");
        jmethodID vpaStart = env->GetMethodID(vpaClass, "start", "()V");
        if (!clearJniException(env) && setAlpha && animate && cancel && vpaAlpha && vpaDelay && vpaDur && vpaStart) {
            jobject anim = env->CallObjectMethod(gOverlay.cardViewRef, animate);
            if (!clearJniException(env) && anim) {
                env->CallVoidMethod(anim, cancel);
                env->CallVoidMethod(gOverlay.cardViewRef, setAlpha, 1.0f);
                jobject a1 = env->CallObjectMethod(anim, vpaAlpha, 0.0f);
                if (a1) {
                    jobject a2 = env->CallObjectMethod(a1, vpaDelay, static_cast<jlong>(3200));
                    if (a2) {
                        jobject a3 = env->CallObjectMethod(a2, vpaDur, static_cast<jlong>(350));
                        if (a3) {
                            env->CallVoidMethod(a3, vpaStart);
                        }
                    }
                }
                (void)clearJniException(env);
            }
        }
    }

    env->PopLocalFrame(nullptr);
#else
    (void)cfg;
#endif
}

void dismissLiveColorPreview() noexcept {
#if defined(__ANDROID__)
    JNIEnv* env = getAttachedUiThreadEnv();
    std::lock_guard<std::mutex> lock(gOverlayMutex);
    releaseOverlayRefsLocked(env, env != nullptr);
#endif
}

} // namespace placepreview::ui

#if defined(__ANDROID__)
extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* /*reserved*/) {
    placepreview::ui::gJavaVm.store(vm, std::memory_order_release);
    return JNI_VERSION_1_6;
}
#endif
