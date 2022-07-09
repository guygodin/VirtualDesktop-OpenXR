// MIT License
//
// Copyright(c) 2022 Matthieu Bucchianeri
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this softwareand associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright noticeand this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "pch.h"

#include "log.h"
#include "runtime.h"
#include "utils.h"

namespace pimax_openxr {

    using namespace pimax_openxr::log;
    using namespace pimax_openxr::utils;
    using namespace xr::math;

    // https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#xrStringToPath
    XrResult OpenXrRuntime::xrStringToPath(XrInstance instance, const char* pathString, XrPath* path) {
        TraceLoggingWrite(g_traceProvider, "xrStringToPath", TLXArg(instance, "Instance"), TLArg(pathString, "String"));

        if (instance != XR_NULL_PATH && (!m_instanceCreated || instance != (XrInstance)1)) {
            return XR_ERROR_HANDLE_INVALID;
        }

        std::string_view str(pathString);

        bool found = false;
        for (auto entry : m_strings) {
            if (entry.second == str) {
                *path = entry.first;
                found = true;
                break;
            }
        }

        if (!found) {
            *path = (XrPath)++m_stringIndex;
            m_strings.insert_or_assign(*path, str);
        }

        TraceLoggingWrite(g_traceProvider, "xrStringToPath", TLArg(*path, "Path"));

        return XR_SUCCESS;
    }

    // https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#xrPathToString
    XrResult OpenXrRuntime::xrPathToString(
        XrInstance instance, XrPath path, uint32_t bufferCapacityInput, uint32_t* bufferCountOutput, char* buffer) {
        TraceLoggingWrite(g_traceProvider,
                          "xrPathToString",
                          TLXArg(instance, "Instance"),
                          TLArg(path, "Path"),
                          TLArg(bufferCapacityInput, "BufferCapacityInput"));

        if (instance != XR_NULL_PATH && (!m_instanceCreated || instance != (XrInstance)1)) {
            return XR_ERROR_HANDLE_INVALID;
        }

        const auto it = m_strings.find(path);
        if (it == m_strings.cend()) {
            return XR_ERROR_PATH_INVALID;
        }

        const auto& str = it->second;
        if (bufferCapacityInput && bufferCapacityInput < str.length()) {
            return XR_ERROR_SIZE_INSUFFICIENT;
        }

        *bufferCountOutput = (uint32_t)str.length() + 1;
        TraceLoggingWrite(g_traceProvider, "xrPathToString", TLArg(*bufferCountOutput, "BufferCountOutput"));

        if (bufferCapacityInput && buffer) {
            sprintf_s(buffer, bufferCapacityInput, "%s", str.c_str());
            TraceLoggingWrite(g_traceProvider, "xrPathToString", TLArg(buffer, "String"));
        }

        return XR_SUCCESS;
    }

    // https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#xrCreateActionSet
    XrResult OpenXrRuntime::xrCreateActionSet(XrInstance instance,
                                              const XrActionSetCreateInfo* createInfo,
                                              XrActionSet* actionSet) {
        if (createInfo->type != XR_TYPE_ACTION_SET_CREATE_INFO) {
            return XR_ERROR_VALIDATION_FAILURE;
        }

        TraceLoggingWrite(g_traceProvider,
                          "xrCreateActionSet",
                          TLXArg(instance, "Instance"),
                          TLArg(createInfo->actionSetName, "Name"),
                          TLArg(createInfo->localizedActionSetName, "LocalizedName"),
                          TLArg(createInfo->priority, "Priority"));

        if (!m_instanceCreated || instance != (XrInstance)1) {
            return XR_ERROR_HANDLE_INVALID;
        }

        // COMPLIANCE: Check for invalid/duplicate name.
        // COMPLIANCE: We do not support the notion of priority.

        *actionSet = (XrActionSet)++m_actionSetIndex;

        // Maintain a list of known actionsets for validation.
        m_actionSets.insert(*actionSet);

        TraceLoggingWrite(g_traceProvider, "xrCreateActionSet", TLXArg(*actionSet, "ActionSet"));

        return XR_SUCCESS;
    }

    // https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#xrDestroyActionSet
    XrResult OpenXrRuntime::xrDestroyActionSet(XrActionSet actionSet) {
        TraceLoggingWrite(g_traceProvider, "xrDestroyActionSet", TLXArg(actionSet, "ActionSet"));

        if (!m_actionSets.count(actionSet)) {
            return XR_ERROR_HANDLE_INVALID;
        }

        m_actionSets.erase(actionSet);

        return XR_SUCCESS;
    }

    // https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#xrCreateAction
    XrResult OpenXrRuntime::xrCreateAction(XrActionSet actionSet,
                                           const XrActionCreateInfo* createInfo,
                                           XrAction* action) {
        if (createInfo->type != XR_TYPE_ACTION_CREATE_INFO) {
            return XR_ERROR_VALIDATION_FAILURE;
        }

        TraceLoggingWrite(g_traceProvider,
                          "xrCreateAction",
                          TLXArg(actionSet, "ActionSet"),
                          TLArg(createInfo->actionName, "Name"),
                          TLArg(createInfo->localizedActionName, "LocalizedName"),
                          TLArg(xr::ToCString(createInfo->actionType), "Type"));
        for (uint32_t i = 0; i < createInfo->countSubactionPaths; i++) {
            TraceLoggingWrite(g_traceProvider,
                              "xrCreateAction",
                              TLArg(getXrPath(createInfo->subactionPaths[i]).c_str(), "SubactionPath"));
        }

        if (!m_actionSets.count(actionSet)) {
            return XR_ERROR_HANDLE_INVALID;
        }

        // COMPLIANCE: Check for invalid/duplicate name.

        // Create the internal struct.
        Action& xrAction = *new Action;
        xrAction.type = createInfo->actionType;
        xrAction.actionSet = actionSet;

        // COMPLIANCE: We do nothing about subActionPaths validation, or actionType.

        *action = (XrAction)&xrAction;

        // Maintain a list of known actionsets for validation.
        m_actions.insert(*action);

        TraceLoggingWrite(g_traceProvider, "xrCreateAction", TLXArg(*action, "Action"));

        return XR_SUCCESS;
    }

    // https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#xrDestroyAction
    XrResult OpenXrRuntime::xrDestroyAction(XrAction action) {
        TraceLoggingWrite(g_traceProvider, "xrDestroyAction", TLXArg(action, "Action"));

        if (!m_actions.count(action)) {
            return XR_ERROR_HANDLE_INVALID;
        }

        // COMPLIANCE: Deleting actions is supposed to be deferred.

        Action* xrAction = (Action*)action;

        delete xrAction;
        m_actions.erase(action);

        return XR_SUCCESS;
    }

    // https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#xrSuggestInteractionProfileBindings
    XrResult OpenXrRuntime::xrSuggestInteractionProfileBindings(
        XrInstance instance, const XrInteractionProfileSuggestedBinding* suggestedBindings) {
        if (suggestedBindings->type != XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING) {
            return XR_ERROR_VALIDATION_FAILURE;
        }

        TraceLoggingWrite(g_traceProvider,
                          "xrSuggestInteractionProfileBindings",
                          TLXArg(instance, "Instance"),
                          TLArg(getXrPath(suggestedBindings->interactionProfile).c_str(), "InteractionProfile"));

        if (!m_instanceCreated || instance != (XrInstance)1) {
            return XR_ERROR_HANDLE_INVALID;
        }

        for (uint32_t i = 0; i < suggestedBindings->countSuggestedBindings; i++) {
            TraceLoggingWrite(g_traceProvider,
                              "xrSuggestInteractionProfileBindings",
                              TLXArg(suggestedBindings->suggestedBindings[i].action, "Action"),
                              TLArg(getXrPath(suggestedBindings->suggestedBindings[i].binding).c_str(), "Path"));
        }

        if (m_activeActionSets.size()) {
            return XR_ERROR_ACTIONSETS_ALREADY_ATTACHED;
        }

        std::vector<XrActionSuggestedBinding> bindings;
        for (uint32_t i = 0; i < suggestedBindings->countSuggestedBindings; i++) {
            // COMPLIANCE: There is no validation of supported/unsupported paths.
            bindings.push_back(suggestedBindings->suggestedBindings[i]);
        }

        m_suggestedBindings.insert_or_assign(getXrPath(suggestedBindings->interactionProfile), bindings);

        return XR_SUCCESS;
    }

    // https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#xrAttachSessionActionSets
    XrResult OpenXrRuntime::xrAttachSessionActionSets(XrSession session,
                                                      const XrSessionActionSetsAttachInfo* attachInfo) {
        if (attachInfo->type != XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO) {
            return XR_ERROR_VALIDATION_FAILURE;
        }

        TraceLoggingWrite(g_traceProvider, "xrAttachSessionActionSets", TLXArg(session, "Session"));
        for (uint32_t i = 0; i < attachInfo->countActionSets; i++) {
            TraceLoggingWrite(
                g_traceProvider, "xrAttachSessionActionSets", TLXArg(attachInfo->actionSets[i], "ActionSet"));
        }

        if (!m_sessionCreated || session != (XrSession)1) {
            return XR_ERROR_HANDLE_INVALID;
        }

        if (m_activeActionSets.size()) {
            return XR_ERROR_ACTIONSETS_ALREADY_ATTACHED;
        }

        for (uint32_t i = 0; i < attachInfo->countActionSets; i++) {
            if (!m_actionSets.count(attachInfo->actionSets[i])) {
                return XR_ERROR_HANDLE_INVALID;
            }

            m_activeActionSets.insert(attachInfo->actionSets[i]);
        }

        return XR_SUCCESS;
    }

    // https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#xrGetCurrentInteractionProfile
    XrResult OpenXrRuntime::xrGetCurrentInteractionProfile(XrSession session,
                                                           XrPath topLevelUserPath,
                                                           XrInteractionProfileState* interactionProfile) {
        if (interactionProfile->type != XR_TYPE_INTERACTION_PROFILE_STATE) {
            return XR_ERROR_VALIDATION_FAILURE;
        }

        TraceLoggingWrite(g_traceProvider,
                          "xrGetCurrentInteractionProfile",
                          TLXArg(session, "Session"),
                          TLArg(getXrPath(topLevelUserPath).c_str(), "TopLevelUserPath"));

        if (!m_sessionCreated || session != (XrSession)1) {
            return XR_ERROR_HANDLE_INVALID;
        }

        // If no side is specified, we use left.
        const int side = topLevelUserPath != XR_NULL_PATH ? getActionSide(getXrPath(topLevelUserPath)) : 0;
        if (side >= 0) {
            interactionProfile->interactionProfile = m_currentInteractionProfile[side];
        } else {
            // Paths we don't support (eg: gamepad).
            interactionProfile->interactionProfile = XR_NULL_PATH;
        }

        TraceLoggingWrite(g_traceProvider,
                          "xrGetCurrentInteractionProfile",
                          TLArg(getXrPath(interactionProfile->interactionProfile).c_str(), "InteractionProfile"));

        return XR_SUCCESS;
    }

    // https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#xrGetActionStateBoolean
    XrResult OpenXrRuntime::xrGetActionStateBoolean(XrSession session,
                                                    const XrActionStateGetInfo* getInfo,
                                                    XrActionStateBoolean* state) {
        if (getInfo->type != XR_TYPE_ACTION_STATE_GET_INFO || state->type != XR_TYPE_ACTION_STATE_BOOLEAN) {
            return XR_ERROR_VALIDATION_FAILURE;
        }

        TraceLoggingWrite(g_traceProvider,
                          "xrGetActionStateBoolean",
                          TLXArg(session, "Session"),
                          TLXArg(getInfo->action, "Action"),
                          TLArg(getXrPath(getInfo->subactionPath).c_str(), "SubactionPath"));

        if (!m_sessionCreated || session != (XrSession)1) {
            return XR_ERROR_HANDLE_INVALID;
        }

        if (!m_actions.count(getInfo->action)) {
            return XR_ERROR_HANDLE_INVALID;
        }

        Action& xrAction = *(Action*)getInfo->action;

        if (xrAction.type != XR_ACTION_TYPE_BOOLEAN_INPUT) {
            return XR_ERROR_ACTION_TYPE_MISMATCH;
        }

        if (!m_activeActionSets.count(xrAction.actionSet)) {
            return XR_ERROR_ACTIONSET_NOT_ATTACHED;
        }

        state->isActive = XR_FALSE;
        state->currentState = xrAction.lastBoolValue;

        if (!xrAction.path.empty()) {
            const std::string fullPath = getActionPath(xrAction, getInfo->subactionPath);
            const bool isBound = xrAction.buttonMap != nullptr || xrAction.floatValue != nullptr;
            TraceLoggingWrite(g_traceProvider,
                              "xrGetActionStateBoolean",
                              TLArg(fullPath.c_str(), "ActionPath"),
                              TLArg(isBound, "Bound"));

            const int side = getActionSide(fullPath);

            // We only support hands paths, not gamepad etc.
            if (isBound && side >= 0) {
                state->isActive = m_isControllerActive[side];
                if (state->isActive && m_frameLatchedActionSets.count(xrAction.actionSet)) {
                    if (xrAction.buttonMap) {
                        state->currentState = xrAction.buttonMap[side] & xrAction.buttonType;
                    } else {
                        state->currentState = xrAction.floatValue[side] > 0.99f;
                    }
                }
            }
        }

        state->changedSinceLastSync = !!state->currentState != xrAction.lastBoolValue;
        state->lastChangeTime = state->changedSinceLastSync ? pvrTimeToXrTime(m_cachedInputState.TimeInSeconds)
                                                            : xrAction.lastBoolValueChangedTime;

        xrAction.lastBoolValue = state->currentState;
        xrAction.lastBoolValueChangedTime = state->lastChangeTime;

        TraceLoggingWrite(g_traceProvider,
                          "xrGetActionStateBoolean",
                          TLArg(!!state->isActive, "Active"),
                          TLArg(!!state->currentState, "CurrentState"),
                          TLArg(!!state->changedSinceLastSync, "ChangedSinceLastSync"),
                          TLArg(state->lastChangeTime, "LastChangeTime"));

        return XR_SUCCESS;
    }

    // https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#xrGetActionStateFloat
    XrResult OpenXrRuntime::xrGetActionStateFloat(XrSession session,
                                                  const XrActionStateGetInfo* getInfo,
                                                  XrActionStateFloat* state) {
        if (getInfo->type != XR_TYPE_ACTION_STATE_GET_INFO || state->type != XR_TYPE_ACTION_STATE_FLOAT) {
            return XR_ERROR_VALIDATION_FAILURE;
        }

        TraceLoggingWrite(g_traceProvider,
                          "xrGetActionStateFloat",
                          TLXArg(session, "Session"),
                          TLXArg(getInfo->action, "Action"),
                          TLArg(getXrPath(getInfo->subactionPath).c_str(), "SubactionPath"));

        if (!m_sessionCreated || session != (XrSession)1) {
            return XR_ERROR_HANDLE_INVALID;
        }

        if (!m_actions.count(getInfo->action)) {
            return XR_ERROR_HANDLE_INVALID;
        }

        Action& xrAction = *(Action*)getInfo->action;

        if (xrAction.type != XR_ACTION_TYPE_FLOAT_INPUT) {
            return XR_ERROR_ACTION_TYPE_MISMATCH;
        }

        if (!m_activeActionSets.count(xrAction.actionSet)) {
            return XR_ERROR_ACTIONSET_NOT_ATTACHED;
        }

        state->isActive = XR_FALSE;
        state->currentState = xrAction.lastFloatValue;

        if (!xrAction.path.empty()) {
            const std::string fullPath = getActionPath(xrAction, getInfo->subactionPath);
            const bool isBound = xrAction.floatValue != nullptr ||
                                 (xrAction.vector2fValue != nullptr && xrAction.vector2fIndex >= 0) ||
                                 xrAction.buttonMap == nullptr;
            TraceLoggingWrite(g_traceProvider,
                              "xrGetActionStateFloat",
                              TLArg(fullPath.c_str(), "ActionPath"),
                              TLArg(isBound, "Bound"));

            const int side = getActionSide(fullPath);

            // We only support hands paths, not gamepad etc.
            if (isBound && side >= 0) {
                state->isActive = m_isControllerActive[side];
                if (state->isActive && m_frameLatchedActionSets.count(xrAction.actionSet)) {
                    if (xrAction.floatValue) {
                        state->currentState = xrAction.floatValue[side];
                    } else if (xrAction.buttonMap) {
                        state->currentState = xrAction.buttonMap[side] & xrAction.buttonType ? 1.f : 0.f;
                    } else {
                        state->currentState = xrAction.vector2fIndex == 0 ? xrAction.vector2fValue[side].x
                                                                          : xrAction.vector2fValue[side].y;
                    }
                }
            }
        }

        state->changedSinceLastSync = state->currentState != xrAction.lastFloatValue;
        state->lastChangeTime = state->changedSinceLastSync ? pvrTimeToXrTime(m_cachedInputState.TimeInSeconds)
                                                            : xrAction.lastFloatValueChangedTime;

        xrAction.lastFloatValue = state->currentState;
        xrAction.lastFloatValueChangedTime = state->lastChangeTime;

        TraceLoggingWrite(g_traceProvider,
                          "xrGetActionStateFloat",
                          TLArg(!!state->isActive, "Active"),
                          TLArg(state->currentState, "CurrentState"),
                          TLArg(!!state->changedSinceLastSync, "ChangedSinceLastSync"),
                          TLArg(state->lastChangeTime, "LastChangeTime"));

        return XR_SUCCESS;
    }

    // https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#xrGetActionStateVector2f
    XrResult OpenXrRuntime::xrGetActionStateVector2f(XrSession session,
                                                     const XrActionStateGetInfo* getInfo,
                                                     XrActionStateVector2f* state) {
        if (getInfo->type != XR_TYPE_ACTION_STATE_GET_INFO || state->type != XR_TYPE_ACTION_STATE_VECTOR2F) {
            return XR_ERROR_VALIDATION_FAILURE;
        }

        TraceLoggingWrite(g_traceProvider,
                          "xrGetActionStateVector2f",
                          TLXArg(session, "Session"),
                          TLXArg(getInfo->action, "Action"),
                          TLArg(getXrPath(getInfo->subactionPath).c_str(), "SubactionPath"));

        if (!m_sessionCreated || session != (XrSession)1) {
            return XR_ERROR_HANDLE_INVALID;
        }

        if (!m_actions.count(getInfo->action)) {
            return XR_ERROR_HANDLE_INVALID;
        }

        Action& xrAction = *(Action*)getInfo->action;

        if (xrAction.type != XR_ACTION_TYPE_VECTOR2F_INPUT) {
            return XR_ERROR_ACTION_TYPE_MISMATCH;
        }

        if (!m_activeActionSets.count(xrAction.actionSet)) {
            return XR_ERROR_ACTIONSET_NOT_ATTACHED;
        }

        state->isActive = XR_FALSE;
        state->currentState = xrAction.lastVector2fValue;

        if (!xrAction.path.empty()) {
            const bool isBound = xrAction.vector2fValue != nullptr;
            const std::string fullPath = getActionPath(xrAction, getInfo->subactionPath);
            TraceLoggingWrite(g_traceProvider,
                              "xrGetActionStateVector2f",
                              TLArg(fullPath.c_str(), "ActionPath"),
                              TLArg(isBound, "Bound"));

            const int side = getActionSide(fullPath);

            // We only support hands paths, not gamepad etc.
            if (isBound && side >= 0) {
                state->isActive = m_isControllerActive[side];
                if (state->isActive && m_frameLatchedActionSets.count(xrAction.actionSet)) {
                    state->currentState.x = xrAction.vector2fValue[side].x;
                    state->currentState.y = xrAction.vector2fValue[side].y;
                }
            }
        }

        state->changedSinceLastSync = state->currentState.x != xrAction.lastVector2fValue.x ||
                                      state->currentState.y != xrAction.lastVector2fValue.y;
        state->lastChangeTime = state->changedSinceLastSync ? pvrTimeToXrTime(m_cachedInputState.TimeInSeconds)
                                                            : xrAction.lastVector2fValueChangedTime;

        xrAction.lastVector2fValue = state->currentState;
        xrAction.lastVector2fValueChangedTime = state->lastChangeTime;

        TraceLoggingWrite(
            g_traceProvider,
            "xrGetActionStateVector2f",
            TLArg(!!state->isActive, "Active"),
            TLArg(fmt::format("{}, {}", state->currentState.x, state->currentState.y).c_str(), "CurrentState"),
            TLArg(!!state->changedSinceLastSync, "ChangedSinceLastSync"),
            TLArg(state->lastChangeTime, "LastChangeTime"));

        return XR_SUCCESS;
    }

    // https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#xrGetActionStatePose
    XrResult OpenXrRuntime::xrGetActionStatePose(XrSession session,
                                                 const XrActionStateGetInfo* getInfo,
                                                 XrActionStatePose* state) {
        if (getInfo->type != XR_TYPE_ACTION_STATE_GET_INFO || state->type != XR_TYPE_ACTION_STATE_POSE) {
            return XR_ERROR_VALIDATION_FAILURE;
        }

        TraceLoggingWrite(g_traceProvider,
                          "xrGetActionStatePose",
                          TLXArg(session, "Session"),
                          TLXArg(getInfo->action, "Action"),
                          TLArg(getXrPath(getInfo->subactionPath).c_str(), "SubactionPath"));

        if (!m_sessionCreated || session != (XrSession)1) {
            return XR_ERROR_HANDLE_INVALID;
        }

        if (!m_actions.count(getInfo->action)) {
            return XR_ERROR_HANDLE_INVALID;
        }

        Action& xrAction = *(Action*)getInfo->action;

        if (xrAction.type != XR_ACTION_TYPE_POSE_INPUT) {
            return XR_ERROR_ACTION_TYPE_MISMATCH;
        }

        if (!m_activeActionSets.count(xrAction.actionSet)) {
            return XR_ERROR_ACTIONSET_NOT_ATTACHED;
        }

        state->isActive = XR_FALSE;
        if (!xrAction.path.empty()) {
            const std::string fullPath = getActionPath(xrAction, getInfo->subactionPath);
            TraceLoggingWrite(g_traceProvider, "xrGetActionStatePose", TLArg(fullPath.c_str(), "ActionPath"));

            const int side = getActionSide(fullPath);

            // We only support hands paths, not gamepad etc.
            if (side >= 0) {
                state->isActive = m_isControllerActive[side];
            }
        }

        TraceLoggingWrite(g_traceProvider, "xrGetActionStatePose", TLArg(!!state->isActive, "Active"));

        return XR_SUCCESS;
    }

    // https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#xrSyncActions
    XrResult OpenXrRuntime::xrSyncActions(XrSession session, const XrActionsSyncInfo* syncInfo) {
        if (syncInfo->type != XR_TYPE_ACTIONS_SYNC_INFO) {
            return XR_ERROR_VALIDATION_FAILURE;
        }

        TraceLoggingWrite(g_traceProvider, "xrSyncActions", TLXArg(session, "Session"));
        for (uint32_t i = 0; i < syncInfo->countActiveActionSets; i++) {
            TraceLoggingWrite(g_traceProvider,
                              "xrSyncActions",
                              TLXArg(syncInfo->activeActionSets[i].actionSet, "ActionSet"),
                              TLArg(syncInfo->activeActionSets[i].subactionPath, "SubactionPath"));
        }

        if (!m_sessionCreated || session != (XrSession)1) {
            return XR_ERROR_HANDLE_INVALID;
        }

        for (uint32_t i = 0; i < syncInfo->countActiveActionSets; i++) {
            if (!m_activeActionSets.count(syncInfo->activeActionSets[i].actionSet)) {
                return XR_ERROR_ACTIONSET_NOT_ATTACHED;
            }

            m_frameLatchedActionSets.insert(syncInfo->activeActionSets[i].actionSet);

            // COMPLIANCE: We do nothing with subActionPath.
        }

        // Latch the state of all inputs, and we will let the further calls to xrGetActionState*() do the triage.
        CHECK_PVRCMD(pvr_getInputState(m_pvrSession, &m_cachedInputState));
        for (uint32_t side = 0; side < 2; side++) {
            TraceLoggingWrite(
                g_traceProvider,
                "PVR_InputState",
                TLArg(side == 0 ? "Left" : "Right", "Side"),
                TLArg(m_cachedInputState.TimeInSeconds, "TimeInSeconds"),
                TLArg(m_cachedInputState.HandButtons[side], "ButtonPress"),
                TLArg(m_cachedInputState.HandTouches[side], "ButtonTouches"),
                TLArg(m_cachedInputState.Trigger[side], "Trigger"),
                TLArg(m_cachedInputState.Grip[side], "Grip"),
                TLArg(m_cachedInputState.GripForce[side], "GripForce"),
                TLArg(fmt::format("{}, {}", m_cachedInputState.JoyStick[side].x, m_cachedInputState.JoyStick[side].y)
                          .c_str(),
                      "Joystick"),
                TLArg(fmt::format("{}, {}", m_cachedInputState.TouchPad[side].x, m_cachedInputState.TouchPad[side].y)
                          .c_str(),
                      "Touchpad"),
                TLArg(m_cachedInputState.TouchPadForce[side], "TouchpadForce"),
                TLArg(m_cachedInputState.fingerIndex[side], "IndexFinger"),
                TLArg(m_cachedInputState.fingerMiddle[side], "MiddleFinger"),
                TLArg(m_cachedInputState.fingerRing[side], "RingFinger"),
                TLArg(m_cachedInputState.fingerPinky[side], "PinkyFinger"));

            const auto lastControllerType = m_cachedControllerType[side];
            const int size = pvr_getTrackedDeviceStringProperty(m_pvrSession,
                                                                side == 0 ? pvrTrackedDevice_LeftController
                                                                          : pvrTrackedDevice_RightController,
                                                                pvrTrackedDeviceProp_ControllerType_String,
                                                                nullptr,
                                                                0);
            m_isControllerActive[side] = size > 0;
            if (m_isControllerActive[side]) {
                m_cachedControllerType[side].resize(size, 0);
                pvr_getTrackedDeviceStringProperty(m_pvrSession,
                                                   side == 0 ? pvrTrackedDevice_LeftController
                                                             : pvrTrackedDevice_RightController,
                                                   pvrTrackedDeviceProp_ControllerType_String,
                                                   m_cachedControllerType[side].data(),
                                                   (int)m_cachedControllerType[side].size() + 1);
                // Remove trailing 0.
                m_cachedControllerType[side].resize(size - 1, 0);
            } else {
                m_cachedControllerType[side].clear();
            }

            if (lastControllerType != m_cachedControllerType[side]) {
                TraceLoggingWrite(g_traceProvider,
                                  "PVR_ControllerType",
                                  TLArg(side == 0 ? "Left" : "Right", "Side"),
                                  TLArg(m_cachedControllerType[side].c_str(), "Type"));
                rebindControllerActions(side);
            }
        }

        return XR_SUCCESS;
    }

    // https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#xrEnumerateBoundSourcesForAction
    XrResult OpenXrRuntime::xrEnumerateBoundSourcesForAction(XrSession session,
                                                             const XrBoundSourcesForActionEnumerateInfo* enumerateInfo,
                                                             uint32_t sourceCapacityInput,
                                                             uint32_t* sourceCountOutput,
                                                             XrPath* sources) {
        if (enumerateInfo->type != XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO) {
            return XR_ERROR_VALIDATION_FAILURE;
        }

        TraceLoggingWrite(g_traceProvider,
                          "xrEnumerateBoundSourcesForAction",
                          TLXArg(session, "Session"),
                          TLXArg(enumerateInfo->action, "Action"),
                          TLArg(sourceCapacityInput, "SourceCapacityInput"));

        if (!m_sessionCreated || session != (XrSession)1) {
            return XR_ERROR_HANDLE_INVALID;
        }

        if (!m_actions.count(enumerateInfo->action)) {
            return XR_ERROR_HANDLE_INVALID;
        }

        Action& xrAction = *(Action*)enumerateInfo->action;

        if (sourceCapacityInput && sourceCapacityInput < (xrAction.path.empty() ? 0u : 1u)) {
            return XR_ERROR_SIZE_INSUFFICIENT;
        }

        *sourceCountOutput = xrAction.path.empty() ? 0 : 1;
        TraceLoggingWrite(
            g_traceProvider, "xrEnumerateBoundSourcesForAction", TLArg(*sourceCountOutput, "SourceCountOutput"));

        if (sourceCapacityInput && sources && !xrAction.path.empty()) {
            CHECK_XRCMD(xrStringToPath(XR_NULL_HANDLE, xrAction.path.c_str(), &sources[0]));
            TraceLoggingWrite(g_traceProvider,
                              "xrEnumerateBoundSourcesForAction",
                              TLArg(sources[0], "Source"),
                              TLArg(xrAction.path.c_str(), "Path"));
        }

        TraceLoggingWrite(
            g_traceProvider, "xrEnumerateBoundSourcesForAction", TLArg(*sourceCountOutput, "SourceCountOutput"));

        return XR_SUCCESS;
    }

    // https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#xrGetInputSourceLocalizedName
    XrResult OpenXrRuntime::xrGetInputSourceLocalizedName(XrSession session,
                                                          const XrInputSourceLocalizedNameGetInfo* getInfo,
                                                          uint32_t bufferCapacityInput,
                                                          uint32_t* bufferCountOutput,
                                                          char* buffer) {
        if (getInfo->type != XR_TYPE_INPUT_SOURCE_LOCALIZED_NAME_GET_INFO) {
            return XR_ERROR_VALIDATION_FAILURE;
        }

        TraceLoggingWrite(g_traceProvider,
                          "xrGetInputSourceLocalizedName",
                          TLXArg(session, "Session"),
                          TLArg(getXrPath(getInfo->sourcePath).c_str(), "SourcePath"),
                          TLArg(getInfo->whichComponents, "WhichComponents"));

        if (!m_sessionCreated || session != (XrSession)1) {
            return XR_ERROR_HANDLE_INVALID;
        }

        // Build the string.
        std::string localizedName;

        const std::string path = getXrPath(getInfo->sourcePath);

        const int side = getActionSide(path);
        if (side >= 0) {
            if ((getInfo->whichComponents & XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT)) {
                localizedName += side == 0 ? "Left Hand " : "Right Hand ";
            }

            if ((getInfo->whichComponents & XR_INPUT_SOURCE_LOCALIZED_NAME_INTERACTION_PROFILE_BIT)) {
                localizedName += m_localizedControllerType[side] + " ";
            }

            if ((getInfo->whichComponents & XR_INPUT_SOURCE_LOCALIZED_NAME_COMPONENT_BIT)) {
                const std::string& interactionProfile = getXrPath(m_currentInteractionProfile[side]);
                if (interactionProfile == "/interaction_profiles/htc/vive_controller") {
                    localizedName += getViveControllerLocalizedSourceName(path);
                } else if (interactionProfile == "/interaction_profiles/valve/index_controller") {
                    localizedName += getIndexControllerLocalizedSourceName(path);
                } else if (interactionProfile == "/interaction_profiles/khr/simple_controller") {
                    localizedName += getSimpleControllerLocalizedSourceName(path);
                }
            }
        }

        if (bufferCapacityInput && bufferCapacityInput < localizedName.length()) {
            return XR_ERROR_SIZE_INSUFFICIENT;
        }

        *bufferCountOutput = (uint32_t)localizedName.length() + 1;
        TraceLoggingWrite(
            g_traceProvider, "xrGetInputSourceLocalizedName", TLArg(*bufferCountOutput, "BufferCountOutput"));

        if (bufferCapacityInput && buffer) {
            sprintf_s(buffer, bufferCapacityInput, "%s", localizedName.c_str());
            TraceLoggingWrite(g_traceProvider, "xrGetInputSourceLocalizedName", TLArg(buffer, "String"));
        }

        return XR_SUCCESS;
    }

    // https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#xrApplyHapticFeedback
    XrResult OpenXrRuntime::xrApplyHapticFeedback(XrSession session,
                                                  const XrHapticActionInfo* hapticActionInfo,
                                                  const XrHapticBaseHeader* hapticFeedback) {
        if (hapticActionInfo->type != XR_TYPE_HAPTIC_ACTION_INFO) {
            return XR_ERROR_VALIDATION_FAILURE;
        }

        TraceLoggingWrite(g_traceProvider,
                          "xrApplyHapticFeedback",
                          TLXArg(session, "Session"),
                          TLXArg(hapticActionInfo->action, "Action"),
                          TLArg(getXrPath(hapticActionInfo->subactionPath).c_str(), "SubactionPath"));

        if (!m_sessionCreated || session != (XrSession)1) {
            return XR_ERROR_HANDLE_INVALID;
        }

        if (!m_actions.count(hapticActionInfo->action)) {
            return XR_ERROR_HANDLE_INVALID;
        }

        Action& xrAction = *(Action*)hapticActionInfo->action;

        if (xrAction.type != XR_ACTION_TYPE_VIBRATION_OUTPUT) {
            return XR_ERROR_ACTION_TYPE_MISMATCH;
        }

        if (!m_activeActionSets.count(xrAction.actionSet)) {
            return XR_ERROR_ACTIONSET_NOT_ATTACHED;
        }

        if (!xrAction.path.empty()) {
            const std::string fullPath = getActionPath(xrAction, hapticActionInfo->subactionPath);
            const bool isOutput = endsWith(fullPath, "/output/haptic");
            TraceLoggingWrite(g_traceProvider, "xrApplyHapticFeedback", TLArg(fullPath.c_str(), "ActionPath"));

            const int side = getActionSide(fullPath);

            // We only support hands paths, not gamepad etc.
            if (isOutput && side >= 0) {
                const XrHapticBaseHeader* entry = reinterpret_cast<const XrHapticBaseHeader*>(hapticFeedback);
                while (entry) {
                    if (entry->type == XR_TYPE_HAPTIC_VIBRATION) {
                        const XrHapticVibration* vibration = reinterpret_cast<const XrHapticVibration*>(entry);

                        TraceLoggingWrite(g_traceProvider,
                                          "xrApplyHapticFeedback",
                                          TLArg(vibration->amplitude, "Amplitude"),
                                          TLArg(vibration->frequency, "Frequency"),
                                          TLArg(vibration->duration, "Duration"));

                        // NOTE: PVR only supports pulses, so there is nothing we can do with the frequency/duration?
                        // OpenComposite seems to pass an amplitude of 0 sometimes, which is not supported.
                        if (vibration->amplitude > 0) {
                            CHECK_PVRCMD(pvr_triggerHapticPulse(m_pvrSession,
                                                                side == 0 ? pvrTrackedDevice_LeftController
                                                                          : pvrTrackedDevice_RightController,
                                                                vibration->amplitude));
                        }
                        break;
                    }

                    entry = reinterpret_cast<const XrHapticBaseHeader*>(entry->next);
                }
            }
        }

        return XR_SUCCESS;
    }

    // https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#xrStopHapticFeedback
    XrResult OpenXrRuntime::xrStopHapticFeedback(XrSession session, const XrHapticActionInfo* hapticActionInfo) {
        if (hapticActionInfo->type != XR_TYPE_HAPTIC_ACTION_INFO) {
            return XR_ERROR_VALIDATION_FAILURE;
        }

        TraceLoggingWrite(g_traceProvider,
                          "xrStopHapticFeedback",
                          TLXArg(session, "Session"),
                          TLXArg(hapticActionInfo->action, "Action"),
                          TLArg(getXrPath(hapticActionInfo->subactionPath).c_str(), "SubactionPath"));

        if (!m_sessionCreated || session != (XrSession)1) {
            return XR_ERROR_HANDLE_INVALID;
        }

        if (!m_actions.count(hapticActionInfo->action)) {
            return XR_ERROR_HANDLE_INVALID;
        }

        Action& xrAction = *(Action*)hapticActionInfo->action;

        if (xrAction.type != XR_ACTION_TYPE_VIBRATION_OUTPUT) {
            return XR_ERROR_ACTION_TYPE_MISMATCH;
        }

        if (!m_activeActionSets.count(xrAction.actionSet)) {
            return XR_ERROR_ACTIONSET_NOT_ATTACHED;
        }

        if (!xrAction.path.empty()) {
            const std::string fullPath = getActionPath(xrAction, hapticActionInfo->subactionPath);
            const bool isOutput = endsWith(fullPath, "/output/haptic");
            TraceLoggingWrite(g_traceProvider, "xrStopHapticFeedback", TLArg(fullPath.c_str(), "ActionPath"));

            const int side = getActionSide(fullPath);

            // We only support hands paths, not gamepad etc.
            if (isOutput && side >= 0) {
                // Nothing to do here.
            }
        }

        return XR_SUCCESS;
    }

    void OpenXrRuntime::rebindControllerActions(int side) {
        std::string preferredInteractionProfile;
        std::string actualInteractionProfile;
        XrPosef aimPose = Pose::Identity();

        // Identify the physical controller type.
        if (m_cachedControllerType[side] == "vive_controller") {
            preferredInteractionProfile = "/interaction_profiles/htc/vive_controller";
            m_localizedControllerType[side] = "Vive Controller";
            aimPose = Pose::MakePose(Quaternion::RotationRollPitchYaw({PVR::DegreeToRad(-45.f), 0, 0}),
                                     XrVector3f{0, 0, -0.05f});
        } else if (m_cachedControllerType[side] == "knuckles") {
            preferredInteractionProfile = "/interaction_profiles/valve/index_controller";
            m_localizedControllerType[side] = "Index Controller";
            aimPose = Pose::MakePose(Quaternion::RotationRollPitchYaw({PVR::DegreeToRad(-70.f), 0, 0}),
                                     XrVector3f{0, 0, -0.05f});
        } else {
            // Fallback to simple controller.
            preferredInteractionProfile = "/interaction_profiles/khr/simple_controller";
            m_localizedControllerType[side] = "Controller";
        }

        // Try to map with the preferred bindings.
        auto bindings = m_suggestedBindings.find(preferredInteractionProfile);
        if (bindings != m_suggestedBindings.cend()) {
            actualInteractionProfile = preferredInteractionProfile;
        } else {
            // In order of preference.
            static const std::string fallbacks[] = {
                "/interaction_profiles/oculus/touch_controller",
                "/interaction_profiles/microsoft/motion_controller",
                "/interaction_profiles/khr/simple_controller",
            };
            for (int i = 0; i < ARRAYSIZE(fallbacks); i++) {
                bindings = m_suggestedBindings.find(fallbacks[i]);
                if (bindings != m_suggestedBindings.cend()) {
                    actualInteractionProfile = fallbacks[i];
                    break;
                }
            }
        }

        // TODO: We don't support multiple bound sources for the same action.

        if (bindings != m_suggestedBindings.cend()) {
            const auto& mapping =
                m_controllerMappingTable.find(std::make_pair(actualInteractionProfile, preferredInteractionProfile))
                    ->second;
            for (const auto& binding : bindings->second) {
                if (!m_actions.count(binding.action)) {
                    continue;
                }

                Action& xrAction = *(Action*)binding.action;

                // Map to the PVR input state.
                mapping(xrAction, binding.binding);
            }
        }

        TraceLoggingWrite(g_traceProvider,
                          "xrSyncActions",
                          TLArg(side == 0 ? "Left" : "Right", "Side"),
                          TLArg(actualInteractionProfile.c_str(), "InteractionProfile"));

        if (!actualInteractionProfile.empty()) {
            CHECK_XRCMD(
                xrStringToPath(XR_NULL_HANDLE, actualInteractionProfile.c_str(), &m_currentInteractionProfile[side]));
            m_controllerAimPose[side] = aimPose;
        } else {
            m_currentInteractionProfile[side] = XR_NULL_PATH;
            m_controllerAimPose[side] = Pose::Identity();
        }

        m_currentInteractionProfileDirty = true;
    }

    std::string OpenXrRuntime::getXrPath(XrPath path) const {
        if (path == XR_NULL_PATH) {
            return "";
        }

        const auto it = m_strings.find(path);
        if (it == m_strings.cend()) {
            return "<unknown>";
        }

        return it->second;
    }

    std::string OpenXrRuntime::getActionPath(const Action& xrAction, XrPath subActionPath) const {
        std::string path;
        if (subActionPath != XR_NULL_PATH) {
            path = getXrPath(subActionPath);
        }

        if (!path.empty() && !endsWith(path, "/") && !startsWith(xrAction.path, "/")) {
            path += "/";
        }

        path += xrAction.path;

        return path;
    }

    int OpenXrRuntime::getActionSide(const std::string& fullPath) const {
        if (startsWith(fullPath, "/user/hand/left")) {
            return 0;
        } else if (startsWith(fullPath, "/user/hand/right")) {
            return 1;
        }

        return -1;
    }

} // namespace pimax_openxr
