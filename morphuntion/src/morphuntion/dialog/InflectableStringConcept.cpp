/*
 * Copyright 2017-2024 Apple Inc. All rights reserved.
 */
#include <morphuntion/dialog/InflectableStringConcept.hpp>

#include <morphuntion/dialog/DefaultDisplayFunction.hpp>
#include <morphuntion/dialog/DefaultFeatureFunction.hpp>
#include <morphuntion/dialog/SemanticFeature.hpp>
#include <morphuntion/dialog/SemanticFeatureModel_DisplayData.hpp>
#include <morphuntion/dialog/SemanticFeatureModel_DisplayValue.hpp>
#include <morphuntion/dialog/SemanticFeatureModel.hpp>
#include <morphuntion/dialog/SpeakableString.hpp>
#include <morphuntion/npc.hpp>
#include <memory>

namespace morphuntion::dialog {

InflectableStringConcept::InflectableStringConcept(const SemanticFeatureModel* model, const SpeakableString& value)
    : super(model)
    , value(value)
    , defaultDisplayValue(value, *npc(super::getSpeakFeature()))
{
}

InflectableStringConcept::InflectableStringConcept(const InflectableStringConcept& other)
    : super(other)
    , value(other.value)
    , defaultDisplayValue(other.defaultDisplayValue)
{
}

InflectableStringConcept::~InflectableStringConcept()
{
}

SpeakableString* InflectableStringConcept::getFeatureValue(const SemanticFeature& feature) const
{
    auto constraint = getConstraint(feature);
    if (constraint != nullptr) {
        return new SpeakableString(*npc(constraint));
    }
    auto defaultFeatureFunction = npc(getModel())->getDefaultFeatureFunction(feature);
    if (defaultFeatureFunction != nullptr) {
        const auto displayValueResult = getDisplayValue(true);
        if (displayValueResult) {
            return npc(defaultFeatureFunction)->getFeatureValue(*displayValueResult, constraints);
        }
    }
    return nullptr;
}

bool InflectableStringConcept::isExists() const
{
    return getDisplayValue(false).has_value();
}

::std::u16string InflectableStringConcept::toString() const
{
    return value.toString();
}

::std::optional<SemanticFeatureModel_DisplayValue> InflectableStringConcept::getDisplayValue(bool allowInflectionGuess) const
{
    auto defaultDisplayFunction = npc(getModel())->getDefaultDisplayFunction();
    if (defaultDisplayFunction != nullptr && !constraints.empty()) {
        ::std::map<SemanticFeature, ::std::u16string> constraintMap;
        if (!value.speakEqualsPrint()) {
            constraintMap.emplace(*npc(getSpeakFeature()), value.getSpeak());
        }
        SemanticFeatureModel_DisplayData displayData({SemanticFeatureModel_DisplayValue(value.getPrint(), constraintMap)});
        ::std::unique_ptr<SemanticFeatureModel_DisplayValue> returnVal(npc(defaultDisplayFunction)->getDisplayValue(displayData, constraints, allowInflectionGuess));
        if (returnVal != nullptr) {
            return *returnVal;
        }
    }
    if (allowInflectionGuess) {
        return defaultDisplayValue;
    }
    return {};
}

SpeakableString* InflectableStringConcept::toSpeakableString() const
{
    auto displayValueResult = getDisplayValue(true);
    if (!displayValueResult) {
        return nullptr;
    }
    const auto& displayValue= *displayValueResult;
    auto speakValue = displayValue.getFeatureValue(*npc(getSpeakFeature()));
    if (speakValue != nullptr) {
        return new SpeakableString(displayValue.getDisplayString(), *npc(speakValue));
    }
    return new SpeakableString(displayValue.getDisplayString());
}

InflectableStringConcept* InflectableStringConcept::clone() const
{
    return new InflectableStringConcept(*this);
}

} // namespace morphuntion::dialog
