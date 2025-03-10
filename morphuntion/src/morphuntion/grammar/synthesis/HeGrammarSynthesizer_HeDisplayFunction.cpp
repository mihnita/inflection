/*
 * Copyright 2017-2024 Apple Inc. All rights reserved.
 */
#include <morphuntion/grammar/synthesis/HeGrammarSynthesizer_HeDisplayFunction.hpp>

#include <morphuntion/tokenizer/TokenChain.hpp>
#include <morphuntion/dictionary/DictionaryMetaData.hpp>
#include <morphuntion/util/Validate.hpp>
#include <morphuntion/util/DelimitedStringIterator.hpp>
#include <morphuntion/util/StringViewUtils.hpp>
#include <morphuntion/util/LocaleUtils.hpp>
#include <morphuntion/util/UnicodeSetUtils.hpp>
#include <morphuntion/dialog/SemanticFeature.hpp>
#include <morphuntion/dialog/SemanticFeatureModel_DisplayData.hpp>
#include <morphuntion/dialog/SemanticFeatureModel_DisplayValue.hpp>
#include <morphuntion/dialog/SemanticFeatureModel.hpp>
#include <morphuntion/grammar/synthesis/GrammarSynthesizerUtil.hpp>
#include <morphuntion/grammar/synthesis/GrammemeConstants.hpp>
#include <morphuntion/npc.hpp>

namespace morphuntion::grammar::synthesis {

static const ::icu4cxx::UnicodeSet& DEFINITENESS_COMPOUND_SPLITTER() {
    static auto DEFINITENESS_COMPOUND_SPLITTER_ = ::morphuntion::util::UnicodeSetUtils::freeze(new ::icu4cxx::UnicodeSet(u"[[:whitespace:]-]"));
    return *npc(DEFINITENESS_COMPOUND_SPLITTER_);
}

HeGrammarSynthesizer_HeDisplayFunction::HeGrammarSynthesizer_HeDisplayFunction(const ::morphuntion::dialog::SemanticFeatureModel& model)
    : super()
    , dictionary(*npc(::morphuntion::dictionary::DictionaryMetaData::createDictionary(::morphuntion::util::LocaleUtils::HEBREW())))
    , inflector(::morphuntion::dictionary::Inflector::getInflector(::morphuntion::util::LocaleUtils::HEBREW()))
    , dictionaryInflector(::morphuntion::util::LocaleUtils::HEBREW(), {
            {GrammemeConstants::POS_NOUN(), GrammemeConstants::POS_ADJECTIVE(), GrammemeConstants::POS_VERB()},
            {GrammemeConstants::NUMBER_SINGULAR(),  GrammemeConstants::NUMBER_PLURAL()},
            {GrammemeConstants::GENDER_MASCULINE(), GrammemeConstants::GENDER_FEMININE()}
    }, {}, true)
    , countFeature(npc(model.getFeature(GrammemeConstants::NUMBER)))
    , genderFeature(npc(model.getFeature(GrammemeConstants::GENDER)))
    , definitenessFeature(npc(model.getFeature(GrammemeConstants::DEFINITENESS)))
{
    ::morphuntion::util::Validate::notNull(dictionary.getBinaryProperties(&nounProperty, {u"noun"}));
    ::morphuntion::util::Validate::notNull(dictionary.getBinaryProperties(&adjectiveProperty,{u"adjective"}));
    ::morphuntion::util::Validate::notNull(dictionary.getBinaryProperties(&genderMask,
                                                                            {GrammemeConstants::GENDER_MASCULINE(),
                                                                             GrammemeConstants::GENDER_FEMININE()}));
    ::morphuntion::util::Validate::notNull(dictionary.getBinaryProperties(&dictionaryPreposition, {u"adposition"}));
}

std::optional<::std::u16string> HeGrammarSynthesizer_HeDisplayFunction::pluralizeFirstWordOfCompoundWithInflector(const ::std::u16string& firstWord) const
{
    return dictionaryInflector.inflectWithOptionalConstraints(firstWord, {GrammemeConstants::NUMBER_PLURAL()}, {GrammemeConstants::DEFINITENESS_CONSTRUCT()});
}

::std::u16string HeGrammarSynthesizer_HeDisplayFunction::pluralizeFirstWordOfCompoundWithHeuristics(const ::std::u16string& firstWord) const
{
    auto pluralizedFirstWord(singleWordStemToPlural(firstWord));
    if (::morphuntion::util::StringViewUtils::endsWith(pluralizedFirstWord, u"ם")) {
        pluralizedFirstWord.resize(pluralizedFirstWord.length() - 1);
    }
    return pluralizedFirstWord;
}

::std::u16string HeGrammarSynthesizer_HeDisplayFunction::inflectCompoundToPlural(const ::std::u16string& stemmedSingular) const
{
    ::std::u16string firstWord;
    ::std::vector<::std::u16string_view> rest;
    auto allNouns = true;
    for (morphuntion::util::DelimitedStringIterator word(stemmedSingular, u" "); word.hasNext(); ++word) {
        if (firstWord.empty()) {
            firstWord = *word;
            continue;
        }
        rest.push_back(*word);

        if (!dictionary.hasAllProperties(*word, nounProperty) || dictionary.hasAllProperties(*word, adjectiveProperty)) {
            allNouns = false;
        }
    }
    if (firstWord.empty() && rest.empty()) {
        return stemmedSingular;
    }
    if (!rest.empty() && allNouns) {
        const auto pluralizedFirstWord = pluralizeFirstWordOfCompoundWithInflector(firstWord).value_or(pluralizeFirstWordOfCompoundWithHeuristics(firstWord));
        
        auto out = pluralizedFirstWord;
        for (const auto& word : rest) {
            out += u' ';
            out += word;
        }
        return out;
    }
    auto out = singleWordStemToPlural(firstWord);
    for (const auto& word : rest) {
        out += u' ';
        out += singleWordStemToPlural(word);
    }
    return out;
}

static const char16_t FINAL_TO_MEDIAL_SUBSTITUTION[][2] = {
    {u'ך', u'כ'},
    {u'ם', u'מ'},
    {u'ן', u'נ'},
    {u'ף', u'פ'},
    {u'ץ', u'צ'}
};

::std::u16string HeGrammarSynthesizer_HeDisplayFunction::singleWordStemToPlural(::std::u16string_view stemmedSingular) const
{
    ::std::u16string pluralStem(stemmedSingular);
    for (auto finalToMedial : FINAL_TO_MEDIAL_SUBSTITUTION) {
        ::std::u16string_view finalToMedialStr(finalToMedial, 1);
        if (::morphuntion::util::StringViewUtils::endsWith(pluralStem, finalToMedialStr)) {
            pluralStem[pluralStem.length() - 1] = finalToMedial[1];
        }
    }
    if (u"איש" == stemmedSingular) {
        pluralStem = u"אנשים";
    }
    else if (u"צל" == stemmedSingular) {
        pluralStem = u"צללים";
    }
    else if (u"אישה" == stemmedSingular) {
        pluralStem = u"נשים";
    }
    else if (u"שנה" == stemmedSingular) {
        pluralStem = u"שנים";
    }
    else if (u"מילה" == stemmedSingular) {
        pluralStem = u"מילים";
    }
    else if (u"בת" == stemmedSingular) {
        pluralStem = u"בנות";
    }
    else if (u"חשבון" == stemmedSingular
        || u"חלון" == stemmedSingular
        || u"ראיון" == stemmedSingular
        || u"ארון" == stemmedSingular
        || u"רעיון" == stemmedSingular
        || u"חילזון" == stemmedSingular
        || u"הגיון" == stemmedSingular
        || u"פתרון" == stemmedSingular
        || u"מקום" == stemmedSingular
        || u"אולם" == stemmedSingular
        || u"אב" == stemmedSingular
        || u"ארמון" == stemmedSingular
        || u"דו״ח" == stemmedSingular
        || u"חשד" == stemmedSingular
        || u"כוח" == stemmedSingular
        || u"סוד" == stemmedSingular
        || u"וילון" == stemmedSingular
        || u"לקוח" == stemmedSingular
        || u"רוח" == stemmedSingular
        || u"קול" == stemmedSingular
        || u"מטבע" == stemmedSingular
        || u"עקרון" == stemmedSingular
        || u"קיר" == stemmedSingular
        || u"שטר" == stemmedSingular
        || u"רחוב" == stemmedSingular
        || u"רגש" == stemmedSingular
        || u"ספק" == stemmedSingular
        || u"קרון" == stemmedSingular)
    {
        pluralStem = pluralStem + u"ות";
    }
    else {
        if (::morphuntion::util::StringViewUtils::endsWith(pluralStem, u"וה")) {
            pluralStem = pluralStem.substr(0, pluralStem.length() - 2) + u"ות";
        } else if (::morphuntion::util::StringViewUtils::endsWith(pluralStem, u"ה")) {
            pluralStem = pluralStem.substr(0, pluralStem.length() - 1) + u"ות";
        } else if (::morphuntion::util::StringViewUtils::endsWith(pluralStem, u"אי")) {
            pluralStem += u"ם";
        } else if (::morphuntion::util::StringViewUtils::endsWith(pluralStem, u"ית")) {
            pluralStem = pluralStem.substr(0, pluralStem.length() - 2) + u"יות";
        } else if (::morphuntion::util::StringViewUtils::endsWith(pluralStem, u"ות")) {
            pluralStem = pluralStem.substr(0, pluralStem.length() - 2) + u"ויות";
        } else if (::morphuntion::util::StringViewUtils::endsWith(pluralStem, u"ת")) {
            pluralStem = pluralStem.substr(0, pluralStem.length() - 1) + u"ות";
        } else if (!pluralStem.empty()) {
            pluralStem += u"ים";
        }
    }

    return pluralStem;
}

::std::u16string HeGrammarSynthesizer_HeDisplayFunction::inflectDisplayString(const ::std::u16string& displayString, const std::u16string &count, const std::u16string &gender) const
{
    const auto inflectedWord = dictionaryInflector.inflect(displayString, {count, gender});
    if (inflectedWord.has_value()) {
        return *inflectedWord;
    }
    // Well, dictionary was not able to inflect it. So let's make a guess.
    if (count == GrammemeConstants::NUMBER_DUAL() || count == GrammemeConstants::NUMBER_PLURAL()) {
        return inflectCompoundToPlural(displayString);
    }
    return displayString;
}

::morphuntion::dialog::SemanticFeatureModel_DisplayValue * HeGrammarSynthesizer_HeDisplayFunction::getDisplayValue(const dialog::SemanticFeatureModel_DisplayData &displayData, const ::std::map<::morphuntion::dialog::SemanticFeature, ::std::u16string> &constraints, bool /*enableInflectionGuess*/) const
{
    ::std::u16string displayString;
    if (!displayData.getValues().empty()) {
        displayString = displayData.getValues()[0].getDisplayString();
    }
    if (displayString.empty()) {
        return nullptr;
    }

    const auto countValue(morphuntion::grammar::synthesis::GrammarSynthesizerUtil::getFeatureValue(constraints, countFeature));
    const auto genderValue(morphuntion::grammar::synthesis::GrammarSynthesizerUtil::getFeatureValue(constraints, genderFeature));
    if (!countValue.empty() || !genderValue.empty()) {
        displayString = inflectDisplayString(displayString, countValue, genderValue);
    }
    auto definitenessValue(morphuntion::grammar::synthesis::GrammarSynthesizerUtil::getFeatureValue(constraints, definitenessFeature));
    if (!definitenessValue.empty()) {
        displayString = applyDefiniteness(displayString, definitenessValue);
    }
    return new ::morphuntion::dialog::SemanticFeatureModel_DisplayValue(displayString, constraints);
}

::std::u16string HeGrammarSynthesizer_HeDisplayFunction::applyDefiniteness(const ::std::u16string& input, ::std::u16string_view definiteness) const
{
    if (!input.empty() && GrammemeConstants::DEFINITENESS_DEFINITE() == definiteness) {
        ::std::u16string output;
        auto beforeTailStartIndex = DEFINITENESS_COMPOUND_SPLITTER().spanBack(input, USET_SPAN_NOT_CONTAINED);
        ::std::u16string tail;
        if (beforeTailStartIndex >= 0) {
            output.append(input, 0, beforeTailStartIndex);
            tail = input.substr(beforeTailStartIndex);
        }
        else {
            tail = input;
        }
        if (!::morphuntion::util::StringViewUtils::startsWith(tail, u"ה")) {
            output += u"ה";
        }
        output += tail;
        return output;
    }
    return input;
}

} // namespace morphuntion::grammar::synthesis
