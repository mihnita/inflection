/*
 * Copyright 2017-2024 Apple Inc. All rights reserved.
 */
#include <morphuntion/grammar/synthesis/DeGrammarSynthesizer_DeDisplayFunction.hpp>

#include <morphuntion/dialog/SemanticFeature.hpp>
#include <morphuntion/dialog/SemanticFeatureModel.hpp>
#include <morphuntion/dialog/SemanticFeatureModel_DisplayData.hpp>
#include <morphuntion/dialog/SemanticFeatureModel_DisplayValue.hpp>
#include <morphuntion/dictionary/DictionaryMetaData.hpp>
#include <morphuntion/exception/ClassCastException.hpp>
#include <morphuntion/grammar/synthesis/DeGrammarSynthesizer.hpp>
#include <morphuntion/grammar/synthesis/GrammarSynthesizerUtil.hpp>
#include <morphuntion/grammar/synthesis/GrammemeConstants.hpp>
#include <morphuntion/util/LocaleUtils.hpp>
#include <morphuntion/util/Logger.hpp>
#include <morphuntion/util/LoggerConfig.hpp>
#include <morphuntion/util/Validate.hpp>
#include "morphuntion/tokenizer/TokenChain.hpp"
#include "morphuntion/tokenizer/Tokenizer.hpp"
#include "morphuntion/tokenizer/TokenizerFactory.hpp"
#include <morphuntion/npc.hpp>
#include <unicode/uchar.h>
#include <algorithm>
#include <memory>

template<typename T, typename U>
static T java_cast(U* u)
{
    if (!u) return static_cast<T>(nullptr);
    auto t = dynamic_cast<T>(u);
    if (!t) throw ::morphuntion::exception::ClassCastException();
    return t;
}

namespace morphuntion::grammar::synthesis {

DeGrammarSynthesizer_DeDisplayFunction::DeGrammarSynthesizer_DeDisplayFunction(const ::morphuntion::dialog::SemanticFeatureModel& model, const ::std::map<int32_t, ::std::u16string_view>& strongSuffixes, const ::std::map<int32_t, ::std::u16string_view>& weakSuffixes, const ::std::map<int32_t, ::std::u16string_view>& mixedSuffixes)
    : super()
    , stemFeature(model.getFeature(u"stem"))
    , caseFeature(model.getFeature(GrammemeConstants::CASE))
    , countFeature(model.getFeature(GrammemeConstants::NUMBER))
    , genderFeature(model.getFeature(GrammemeConstants::GENDER))
    , declensionFeature(model.getFeature(DeGrammarSynthesizer::DECLENSION()))
    , partOfSpeechFeature(model.getFeature(GrammemeConstants::POS))
    , strongSuffixes(strongSuffixes)
    , weakSuffixes(weakSuffixes)
    , mixedSuffixes(mixedSuffixes)
    , dictionary(*npc(::morphuntion::dictionary::DictionaryMetaData::createDictionary(::morphuntion::util::LocaleUtils::GERMAN())))
    , inflector(::morphuntion::dictionary::Inflector::getInflector(::morphuntion::util::LocaleUtils::GERMAN()))
    , dictionaryInflector(::morphuntion::util::LocaleUtils::GERMAN(), {
        {GrammemeConstants::POS_DETERMINER(), GrammemeConstants::POS_NOUN(), GrammemeConstants::POS_PROPER_NOUN(), GrammemeConstants::POS_ADJECTIVE()},

        {GrammemeConstants::CASE_NOMINATIVE(),  GrammemeConstants::CASE_ACCUSATIVE(), GrammemeConstants::CASE_GENITIVE(), GrammemeConstants::CASE_DATIVE()},
        {GrammemeConstants::NUMBER_SINGULAR(),  GrammemeConstants::NUMBER_PLURAL()},
        {DeGrammarSynthesizer::DECLENSION_STRONG(), DeGrammarSynthesizer::DECLENSION_MIXED(), DeGrammarSynthesizer::DECLENSION_WEAK()},
        {GrammemeConstants::GENDER_MASCULINE(), GrammemeConstants::GENDER_FEMININE()}
    }, {}, true)
    , definiteArticleLookupFunction(model, true, *npc(java_cast<const DeGrammarSynthesizer_ArticleLookupFunction*>(model.getDefaultFeatureFunction(*npc(model.getFeature(DeGrammarSynthesizer::DEF_ARTICLE))))))
    , indefiniteArticleLookupFunction(model, true, *npc(java_cast<const DeGrammarSynthesizer_ArticleLookupFunction*>(model.getDefaultFeatureFunction(*npc(model.getFeature(DeGrammarSynthesizer::INDEF_ARTICLE))))))
    , definitenessDisplayFunction(model, &definiteArticleLookupFunction, DeGrammarSynthesizer::DEF_ARTICLE, &indefiniteArticleLookupFunction, DeGrammarSynthesizer::INDEF_ARTICLE)
    , tokenizer(::morphuntion::tokenizer::TokenizerFactory::createTokenizer(::morphuntion::util::LocaleUtils::GERMAN()))
{
    ::morphuntion::util::Validate::notNull(dictionary.getBinaryProperties(&dictionaryAdjective, {GrammemeConstants::POS_ADJECTIVE()}));
    ::morphuntion::util::Validate::notNull(dictionary.getBinaryProperties(&dictionaryNoun, {GrammemeConstants::POS_NOUN()}));
    ::morphuntion::util::Validate::notNull(dictionary.getBinaryProperties(&dictionaryVerb, {GrammemeConstants::POS_VERB()}));
    ::morphuntion::util::Validate::notNull(dictionary.getBinaryProperties(&dictionaryDeterminer, {GrammemeConstants::POS_DETERMINER()}));
    ::morphuntion::util::Validate::notNull(dictionary.getBinaryProperties(&dictionaryFeminine, {GrammemeConstants::GENDER_FEMININE()}));
    ::morphuntion::util::Validate::notNull(dictionary.getBinaryProperties(&dictionaryMasculine, {GrammemeConstants::GENDER_MASCULINE()}));
    ::morphuntion::util::Validate::notNull(dictionary.getBinaryProperties(&dictionaryNeuter, {GrammemeConstants::GENDER_NEUTER()}));

    ::morphuntion::util::Validate::notNull(dictionary.getBinaryProperties(&dictionaryNominative, {GrammemeConstants::CASE_NOMINATIVE()}));
    ::morphuntion::util::Validate::notNull(dictionary.getBinaryProperties(&dictionaryGenitive, {GrammemeConstants::CASE_GENITIVE()}));
    ::morphuntion::util::Validate::notNull(dictionary.getBinaryProperties(&dictionaryDative, {GrammemeConstants::CASE_DATIVE()}));
    ::morphuntion::util::Validate::notNull(dictionary.getBinaryProperties(&dictionaryAccusative, {GrammemeConstants::CASE_ACCUSATIVE()}));

    ::morphuntion::util::Validate::notNull(dictionary.getBinaryProperties(&dictionarySingular, {GrammemeConstants::NUMBER_SINGULAR()}));
    ::morphuntion::util::Validate::notNull(dictionary.getBinaryProperties(&dictionaryPlural, {GrammemeConstants::NUMBER_PLURAL()}));

    dictionaryGenderMask = dictionaryFeminine | dictionaryMasculine | dictionaryNeuter;
    dictionaryCaseMask = dictionaryNominative | dictionaryGenitive | dictionaryDative | dictionaryAccusative;
    dictionaryCountMask = dictionarySingular | dictionaryPlural;

}

DeGrammarSynthesizer_DeDisplayFunction::~DeGrammarSynthesizer_DeDisplayFunction()
{
}

const ::std::map<int32_t, ::std::u16string_view>* DeGrammarSynthesizer_DeDisplayFunction::getSuffixMap(::std::u16string_view declensionClass) const
{
    if (declensionClass == DeGrammarSynthesizer::DECLENSION_STRONG()) {
        return &strongSuffixes;
    }
    else if (declensionClass == DeGrammarSynthesizer::DECLENSION_WEAK()) {
        return &weakSuffixes;
    }
    else if (declensionClass == DeGrammarSynthesizer::DECLENSION_MIXED()) {
        return &mixedSuffixes;
    }
    return nullptr;
}

morphuntion::dialog::SemanticFeatureModel_DisplayValue* DeGrammarSynthesizer_DeDisplayFunction::inflectByDeclension(const ::morphuntion::dialog::SemanticFeatureModel_DisplayData& displayData, const ::std::map<::morphuntion::dialog::SemanticFeature, ::std::u16string>& constraints, const ::std::u16string& declensionString) const
{
    auto const suffixMap = getSuffixMap(declensionString);
    if (suffixMap == nullptr) {
        return nullptr;
    }
    ::std::u16string caseString(GrammarSynthesizerUtil::getFeatureValue(constraints, caseFeature));
    ::std::u16string countString(GrammarSynthesizerUtil::getFeatureValue(constraints, countFeature));
    ::std::u16string genderString(GrammarSynthesizerUtil::getFeatureValue(constraints, genderFeature));
    const ::morphuntion::dialog::SemanticFeatureModel_DisplayValue* stemmedValue = nullptr;

    for (const auto& value : displayData.getValues()) {
        auto valueConstraintMap = value.getConstraintMap();
        // Exact match is preferred over stem
        if ((caseString.empty() || caseString == GrammarSynthesizerUtil::getFeatureValue(valueConstraintMap, caseFeature))
            && (countString.empty() || countString == GrammarSynthesizerUtil::getFeatureValue(valueConstraintMap, countFeature))
            && (genderString.empty() || genderString == GrammarSynthesizerUtil::getFeatureValue(valueConstraintMap, genderFeature))
            && valueConstraintMap.find(*npc(declensionFeature)) == valueConstraintMap.end())
        {
            return new ::morphuntion::dialog::SemanticFeatureModel_DisplayValue(value.getDisplayString(), value.getConstraintMap());
        }

        if (valueConstraintMap.find(*npc(stemFeature)) != valueConstraintMap.end() && stemmedValue == nullptr) {
            stemmedValue = &value;
        }
    }

    if (!stemmedValue) {
        return nullptr;
    }

    auto stem = *npc(stemmedValue)->getFeatureValue(*npc(stemFeature));
    if (stem.empty()) {
        stem = npc(stemmedValue)->getDisplayString();
    }

    const auto result = getLookupDeclensionAdjective(stem,
                                        constraints,
                                        GrammarSynthesizerUtil::getFeatureValue(constraints, genderFeature));

    ::std::map<::morphuntion::dialog::SemanticFeature, ::std::u16string> formConstraints;
    formConstraints.insert(npc(stemmedValue)->getConstraintMap().begin(), npc(stemmedValue)->getConstraintMap().end());
    formConstraints.insert(constraints.begin(), constraints.end());

    return new ::morphuntion::dialog::SemanticFeatureModel_DisplayValue(result, formConstraints);
}

::std::optional<::std::u16string> DeGrammarSynthesizer_DeDisplayFunction::inflectWord(const ::std::u16string &displayString, const ::std::map<::morphuntion::dialog::SemanticFeature, ::std::u16string> &constraints, const ::std::vector<::std::u16string> &deducedConstraints, bool enableInflectionGuess) const
{
    // The deduced constraints should take precedence over what's in the constraints, as it has incorporated them already and is more accurate.
    // In case there are none, we convert the constraints to a vector of strings ourselves:
    auto constraintsVec = deducedConstraints;
    if (deducedConstraints.empty()) {
        int64_t binaryType = 0;
        dictionary.getCombinedBinaryType(&binaryType, displayString);

        // Declension feature must not be set for nouns, even if it's included in the constraints.
        // Otherwise the inflection logic would never be able to find a matching inflection patterns as this grammeme can't be applied to nouns:
        ::std::vector<const ::morphuntion::dialog::SemanticFeature*> expectedFeatures;
        if ((binaryType & dictionaryAdjective) != 0) {
            expectedFeatures = {countFeature, genderFeature, caseFeature, declensionFeature};
        } else {
            expectedFeatures = {countFeature, genderFeature, caseFeature};
        }

        constraintsVec = GrammarSynthesizerUtil::convertToStringConstraints(constraints, expectedFeatures);
    }

    const auto dismbiguationGrammemeValues(GrammarSynthesizerUtil::convertToStringConstraints(constraints, {partOfSpeechFeature}));
    auto inflectionResult = dictionaryInflector.inflect(displayString, constraintsVec, dismbiguationGrammemeValues);
    if (inflectionResult) {
        return inflectionResult;
    }

    if (enableInflectionGuess) {
        // Fallback to old declension inflection in case it's an adjective:
        return getLookupDeclensionAdjective(displayString, constraints, GrammarSynthesizerUtil::getFeatureValue(constraints, genderFeature));
    }

    return { };
}

static void filterGrammemesFromSetThatDontContainGrammeme(std::vector<int64_t> &grammemes, int64_t requiredGrammeme)
{
    grammemes.erase(std::remove_if(grammemes.begin(), grammemes.end(), [requiredGrammeme](const int64_t grammemes) {
        return (grammemes & requiredGrammeme) == 0;
    }), grammemes.end());
}

std::optional<::std::pair<::std::u16string, ::std::u16string>> DeGrammarSynthesizer_DeDisplayFunction::inflectDeterminerAndNoun(const ::std::u16string &determiner, const ::std::u16string &noun, const ::std::map<::morphuntion::dialog::SemanticFeature, ::std::u16string>& constraints, bool enableInflectionGuess) const
{
    /*
     The idea here is to extract the most likely case that the determiner is in and
     the most likely count and gender that the noun is in (specified external constraints take precedence).
     The resulting constraints (deducedConstraints) are then used to inflect the entire phrase.
     */

    auto propertiesDependentWord = dictionaryInflector.getwordGrammemesets(determiner);
    auto propertiesHeadWord = dictionaryInflector.getwordGrammemesets(noun);

    // Keep only the grammemes for "POS determiner" around (noun respectively)
    filterGrammemesFromSetThatDontContainGrammeme(propertiesDependentWord, dictionaryDeterminer);
    filterGrammemesFromSetThatDontContainGrammeme(propertiesHeadWord, dictionaryNoun);

    if (::morphuntion::util::LoggerConfig::isTraceEnabled()) {
        ::morphuntion::util::Logger::trace(::std::u16string(u"\nPossible Grammemes for ") + determiner);
        for (auto grammeme : propertiesDependentWord) {
            ::morphuntion::util::Logger::trace(morphuntion::util::StringViewUtils::join(dictionary.getPropertyNames(grammeme), u",") + u"\n");
        }
    }

    // If there's ambiguity, find the most likely set of grammemes:
    const auto grammemesComparator = [&](const int64_t lhs, const int64_t rhs) {
        return dictionaryInflector.compareGrammemes(lhs, rhs) < 0;
    };

    const auto bestDependent = ::std::min_element(propertiesDependentWord.begin(), propertiesDependentWord.end(), grammemesComparator);
    const auto bestHeadWord = ::std::min_element(propertiesHeadWord.begin(), propertiesHeadWord.end(), grammemesComparator);

    // Use the best options to extract constraints:
    ::std::vector<::std::u16string> deducedConstraints;

    if (bestDependent != propertiesDependentWord.end()) {
        const auto bestCase = getFeatureNameFromConstraintsOrBinaryType(constraints, *bestDependent, dictionaryCaseMask, caseFeature);
        if (bestCase) {
            deducedConstraints.emplace_back(*bestCase);
        }
    }

    if (bestHeadWord != propertiesHeadWord.end()) {

        const auto bestGender = getFeatureNameFromConstraintsOrBinaryType(constraints, *bestHeadWord, dictionaryGenderMask, genderFeature);
        if (bestGender) {
            deducedConstraints.emplace_back(*bestGender);
        }
        const auto bestCount = getFeatureNameFromConstraintsOrBinaryType(constraints, *bestHeadWord, dictionaryCountMask, countFeature);
        if (bestCount) {
            deducedConstraints.emplace_back(*bestCount);
        }
    }

    // Apply the deduced constraints to the entire phrase:
    const auto inflectedHeadWord = inflectWord(noun, constraints, deducedConstraints, enableInflectionGuess);

    const auto inflectedDependentWord = inflectWord(determiner, constraints, deducedConstraints, enableInflectionGuess);

    if (!inflectedHeadWord || !inflectedDependentWord) {
        return {};
    }

    return ::std::pair<::std::u16string, ::std::u16string>(*inflectedHeadWord, *inflectedDependentWord);
}

::std::optional<::std::u16string> DeGrammarSynthesizer_DeDisplayFunction::inflectAdjectiveNextToNoun(const ::std::u16string &adjective, const ::std::map<::morphuntion::dialog::SemanticFeature, ::std::u16string> &constraints, const ::std::u16string &nounAfterInflection) const
{
    const ::std::u16string caseString(GrammarSynthesizerUtil::getFeatureValue(constraints, caseFeature));
    ::std::u16string countString(GrammarSynthesizerUtil::getFeatureValue(constraints, countFeature));
    ::std::u16string genderString;
    ::std::u16string declensionString(GrammarSynthesizerUtil::getFeatureValue(constraints, declensionFeature));


    // Plural takes the role of a forth gender in declension, and takes precedence over grammatical gender of the noun.
    if (countString == GrammemeConstants::NUMBER_PLURAL()) {
        genderString = u"";
    } else {
        // Use gender of the noun to avoid grammatical disagreements (should match gender of the constraints):
        int64_t nounAfterInflectionBinaryType = 0;
        dictionary.getCombinedBinaryType(&nounAfterInflectionBinaryType, nounAfterInflection);
        genderString = getGender(constraints, nounAfterInflectionBinaryType);
    }

    // We need a declension class for inflecting adjectives when next to nouns. Fill in that grammeme:
    if (declensionString.empty()) {
        declensionString = DeGrammarSynthesizer::DECLENSION_STRONG();
    }
    // With that, the constraints are all set:
    const auto constraintsVec = { caseString, countString, genderString, declensionString };

    const auto dismbiguationGrammemeValues(GrammarSynthesizerUtil::convertToStringConstraints(constraints, {partOfSpeechFeature}));

    auto inflectionResult = dictionaryInflector.inflect(adjective, constraintsVec, dismbiguationGrammemeValues);
    if (inflectionResult) {
        return inflectionResult;
    }

    return getLookupDeclensionAdjective(adjective, constraints, genderString);
}

::std::u16string DeGrammarSynthesizer_DeDisplayFunction::getGender(const ::std::map<::morphuntion::dialog::SemanticFeature, ::std::u16string>& constraints, int64_t binaryType) const
{
    ::std::u16string genderString;
    if (binaryType != 0) {
        if ((binaryType & dictionaryGenderMask) == dictionaryFeminine) {
            genderString = GrammemeConstants::GENDER_FEMININE();
        } else if ((binaryType & dictionaryGenderMask) == dictionaryMasculine) {
            genderString = GrammemeConstants::GENDER_MASCULINE();
        } else if ((binaryType & dictionaryGenderMask) == dictionaryNeuter) {
            genderString = GrammemeConstants::GENDER_NEUTER();
        }
    }
    if (genderString.empty()) {
        genderString = GrammarSynthesizerUtil::getFeatureValue(constraints, genderFeature);
    }
    return genderString;
}

std::optional<::std::u16string> DeGrammarSynthesizer_DeDisplayFunction::getFeatureNameFromConstraintsOrBinaryType(const ::std::map<::morphuntion::dialog::SemanticFeature, ::std::u16string>& constraints, int64_t binaryType, int64_t mask, const ::morphuntion::dialog::SemanticFeature* semanticFeature) const
{
    // Constraints take priority
    const auto featureString = GrammarSynthesizerUtil::getFeatureValue(constraints, semanticFeature);
    if (!featureString.empty()) {
        return featureString;
    }

    if (binaryType != 0) {
        const auto names = dictionary.getPropertyNames(binaryType & mask);
        if (!names.empty()) {
            return names.front();
        }
    }

    return {};
}

::std::u16string DeGrammarSynthesizer_DeDisplayFunction::getLookupDeclensionAdjective(const ::std::u16string &lemma, const ::std::map<::morphuntion::dialog::SemanticFeature, ::std::u16string> &constraints, const ::std::u16string &targetGender) const
{
    const auto targetDeclension(GrammarSynthesizerUtil::getFeatureValue(constraints, declensionFeature));
    if (!targetDeclension.empty()) {
        auto const suffixMap = getSuffixMap(targetDeclension);
        if (suffixMap != nullptr) {
            ::std::u16string targetCount(GrammarSynthesizerUtil::getFeatureValue(constraints, countFeature));
            ::std::u16string targetCase(GrammarSynthesizerUtil::getFeatureValue(constraints, caseFeature));
            auto caseValue = DeGrammarSynthesizer::getCase(&targetCase);
            auto countValue = DeGrammarSynthesizer::getCount(&targetCount);
            auto genderValue = DeGrammarSynthesizer::getGender(&targetGender);
            auto key = DeGrammarSynthesizer::makeLookupKey(countValue, genderValue, caseValue).value;
            auto suffix = npc(suffixMap)->find(key);

            if (suffix != npc(suffixMap)->end()) {
                ::std::u16string result(lemma);
                result.append(suffix->second);
                return result;
            }
        }
    }
    return lemma;
}

::std::u16string DeGrammarSynthesizer_DeDisplayFunction::inflectGenitiveProperNoun(const std::u16string &displayString) const{
    auto length = displayString.length();
    auto suffix = u_tolower(displayString[length - 1]);
    auto two_letter_suffix = u_tolower(displayString[length - 2]);
    if (suffix == u's' || suffix == u'z' || suffix == u'x' || suffix == u'ß' || (suffix == u'e' && two_letter_suffix == u'c')) {
        return displayString + u"’";
    } else if (suffix != u'’') {
        return displayString + u's';
    }
    return displayString;
}

::morphuntion::dialog::SemanticFeatureModel_DisplayValue * DeGrammarSynthesizer_DeDisplayFunction::getDisplayValue(const dialog::SemanticFeatureModel_DisplayData &displayData, const ::std::map<::morphuntion::dialog::SemanticFeature, ::std::u16string> &constraints, bool enableInflectionGuess) const
{
    ::std::u16string displayString;
    if (!displayData.getValues().empty()) {
        displayString = displayData.getValues()[0].getDisplayString();
    }
    if (displayString.empty()) {
        return nullptr;
    }
    if (!constraints.empty()) {
        // It's not a known word and not a token chain, apply declension suffix if applicable:
        ::std::u16string declensionString(GrammarSynthesizerUtil::getFeatureValue(constraints, declensionFeature));
        if (!declensionString.empty()) {
            auto displayValue = inflectByDeclension(displayData, constraints, declensionString);
            if (displayValue != nullptr) {
                return definitenessDisplayFunction.addDefiniteness(displayValue, constraints);
            }
        }

        ::std::u16string posString(GrammarSynthesizerUtil::getFeatureValue(constraints, partOfSpeechFeature));
        ::std::u16string caseString(GrammarSynthesizerUtil::getFeatureValue(constraints, caseFeature));
        auto length = displayString.length();
        ::std::optional<::std::u16string> inflectionResult;
        if (GrammemeConstants::CASE_GENITIVE() == caseString && posString == GrammemeConstants::POS_PROPER_NOUN() && length > 1) {
            inflectionResult = inflectGenitiveProperNoun(displayString);
        } else if (dictionary.isKnownWord(displayString)) {
            const auto inflectedString = inflectWord(displayString, constraints, {}, enableInflectionGuess);
            if (inflectedString) {
                inflectionResult = *inflectedString;
            }
        } else {
            ::std::unique_ptr<::morphuntion::tokenizer::TokenChain> tokenChain(npc(npc(tokenizer.get())->createTokenChain(displayString)));
            const auto tokenChainInflectionResult = inflectTokenChain(*tokenChain, constraints, enableInflectionGuess);
            if (tokenChainInflectionResult) {
                inflectionResult = *tokenChainInflectionResult;
            }
        }
        if (inflectionResult) {
            displayString = *inflectionResult;
        } else if (!enableInflectionGuess){
            return nullptr;
        }
    }
    if (!displayString.empty()) {
        return definitenessDisplayFunction.addDefiniteness(new ::morphuntion::dialog::SemanticFeatureModel_DisplayValue(displayString, constraints), constraints);
    }
    return nullptr;
}

std::optional<::std::pair<::std::u16string, ::std::u16string>> DeGrammarSynthesizer_DeDisplayFunction::inflect2Words(const ::std::u16string &dependentWord, const ::std::u16string &headWord, const ::std::map<::morphuntion::dialog::SemanticFeature, ::std::u16string> &constraints, bool enableInflectionGuess) const {

    int64_t headBinaryType = 0;
    int64_t dependentBinaryType = 0;
    dictionary.getCombinedBinaryType(&headBinaryType, headWord);
    dictionary.getCombinedBinaryType(&dependentBinaryType, dependentWord);

    // Find out what POS we are looking at.
    if (((dependentBinaryType & dictionaryDeterminer) != 0) && ((headBinaryType & dictionaryNoun) != 0)) {

        return inflectDeterminerAndNoun(dependentWord, headWord, constraints, enableInflectionGuess);
    }

    // From this point on, we need the inflected head word in all cases:
    const auto inflectedHeadWord = inflectWord(headWord, constraints, {}, enableInflectionGuess);
    if (!inflectedHeadWord) {
        return {};
    }

    if (((dependentBinaryType & dictionaryAdjective) != 0) && (dependentBinaryType & dictionaryNoun) == 0 && (headBinaryType & dictionaryNoun) != 0) {

        const auto inflectedDependentWord = inflectAdjectiveNextToNoun(dependentWord, constraints, *inflectedHeadWord);

        // Return nothing to indicate that inflection failed:
        if (!inflectedDependentWord) {
            return {};
        }

        return ::std::pair<::std::u16string, ::std::u16string>(*inflectedHeadWord, *inflectedDependentWord);

    } else if (((dependentBinaryType & dictionaryVerb) != 0) && (dependentBinaryType & dictionaryNoun) == 0 && (headBinaryType & dictionaryNoun) != 0) {

        auto headWordGender = getGender(constraints, headBinaryType);
        const auto inflectedDependentWord = getLookupDeclensionAdjective(dependentWord, constraints, headWordGender);

        return ::std::pair<::std::u16string, ::std::u16string>(*inflectedHeadWord, inflectedDependentWord);
    }

    // Other cases are not handled, return the original dependent word:
    return ::std::pair<::std::u16string, ::std::u16string>(*inflectedHeadWord, dependentWord);
}

::std::optional<::std::u16string> DeGrammarSynthesizer_DeDisplayFunction::inflectTokenChain(const ::morphuntion::tokenizer::TokenChain &tokenChain, const ::std::map<::morphuntion::dialog::SemanticFeature, ::std::u16string> &constraints, bool enableInflectionGuess) const
{
    const auto* headWordToken = npc(npc(tokenChain.getEnd())->getPrevious());
    const auto& headWord = headWordToken->getValue();

    if (headWord.empty()) {
        return {};
    }

    // Find the first token that is before the head-word token, which is not whitespace
    // liebevolle Familienorganisatorin
    // |liebe|volle| |Familien|organisatorin|
    //          ^this word is the dependent word that requires inflection agreeing with "organisatorin"
    auto iteratedToken = headWordToken;
    const morphuntion::tokenizer::Token* dependentToken = nullptr;
    while (iteratedToken != nullptr) {
        // As soon as we discover whitespace, we assume the previous token is the dependent word
        if (iteratedToken->isWhitespace()) {
            dependentToken = npc(iteratedToken)->getPrevious();
            break;
        }
        iteratedToken = npc(iteratedToken)->getPrevious();
    }

    std::optional<::std::u16string> inflectedHeadWord;
    std::optional<::std::u16string> inflectedDependentWord;

    // Check which scenario we have: either 2 words, or a single compound word:
    if (dependentToken != nullptr && npc(dependentToken)->isSignificant()) {
        auto const& dependentWord = dependentToken->getValue();

        const auto inflectionResult = inflect2Words(dependentWord, headWord, constraints, enableInflectionGuess);

        if (!inflectionResult) {
            // inflection has failed
            return {};
        }

        inflectedHeadWord = (*inflectionResult).first;
        inflectedDependentWord = (*inflectionResult).second;
    } else {
        inflectedHeadWord = inflectWord(headWord, constraints, {}, enableInflectionGuess);

        if (!inflectedHeadWord) {
            return { };
        }
    }
    

    // Postflight: put all tokens back together:
    ::std::u16string inflectedResult;
    for (const auto& token : tokenChain) {
        // Use the inflected words rather than the original tokens:
        if (&token == headWordToken && inflectedHeadWord) {
            inflectedResult.append(*inflectedHeadWord);
        } else if (&token == dependentToken && inflectedDependentWord) {
            inflectedResult.append(*inflectedDependentWord);
        } else {
            inflectedResult.append(token.getValue());
        }
    }

    return inflectedResult;
}

} // namespace morphuntion::grammar::synthesis
