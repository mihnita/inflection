/*
 * Copyright 2017-2024 Apple Inc. All rights reserved.
 */
#include <morphuntion/grammar/synthesis/TrGrammarSynthesizer_TrDisplayFunction.hpp>

#include <morphuntion/dialog/DefaultArticleLookupFunction.hpp>
#include <morphuntion/dialog/NumberConcept.hpp>
#include <morphuntion/dialog/SemanticFeature.hpp>
#include <morphuntion/dialog/SemanticFeatureModel.hpp>
#include <morphuntion/dialog/SemanticFeatureModel_DisplayData.hpp>
#include <morphuntion/dialog/SemanticFeatureModel_DisplayValue.hpp>
#include <morphuntion/dictionary/DictionaryMetaData.hpp>
#include <morphuntion/dictionary/PhraseProperties.hpp>
#include <morphuntion/grammar/synthesis/GrammemeConstants.hpp>
#include <morphuntion/grammar/synthesis/TrGrammarSynthesizer.hpp>
#include <morphuntion/util/LocaleUtils.hpp>
#include <morphuntion/util/StringUtils.hpp>
#include <morphuntion/util/StringViewUtils.hpp>
#include <morphuntion/util/UnicodeSetUtils.hpp>
#include <morphuntion/tokenizer/TokenChain.hpp>
#include <morphuntion/tokenizer/Tokenizer.hpp>
#include <morphuntion/tokenizer/TokenizerFactory.hpp>
#include <icu4cxx/NumberFormat.hpp>
#include <unicode/uchar.h>
#include <morphuntion/npc.hpp>
#include <float.h>
#include <memory>

namespace morphuntion::grammar::synthesis {

TrGrammarSynthesizer_TrDisplayFunction::TrGrammarSynthesizer_TrDisplayFunction(const ::morphuntion::dialog::SemanticFeatureModel& model)
    : super()
    , dictionary(*npc(::morphuntion::dictionary::DictionaryMetaData::createDictionary(::morphuntion::util::LocaleUtils::TURKISH())))
    , tokenizer(npc(::morphuntion::tokenizer::TokenizerFactory::createTokenizer(::morphuntion::util::LocaleUtils::TURKISH())))
    , caseFeature(*npc(model.getFeature(GrammemeConstants::CASE)))
    , countFeature(*npc(model.getFeature(GrammemeConstants::NUMBER)))
    , personFeature(*npc(model.getFeature(GrammemeConstants::PERSON)))
    , pronounFeature(*npc(model.getFeature(TrGrammarSynthesizer::PRONOUN())))
    , pronounNumberFeature(*npc(model.getFeature(TrGrammarSynthesizer::PRONOUN_NUMBER())))
    , copulaFeature(*npc(model.getFeature(TrGrammarSynthesizer::COPULA())))
    , tenseFeature(*npc(model.getFeature(TrGrammarSynthesizer::TENSE())))
{
    ::morphuntion::util::Validate::notNull(dictionary.getBinaryProperties(&frontRoundProperty, {u"front-round"}));
    ::morphuntion::util::Validate::notNull(dictionary.getBinaryProperties(&frontUnroundProperty, {u"front-unround"}));
    ::morphuntion::util::Validate::notNull(dictionary.getBinaryProperties(&backRoundProperty, {u"back-round"}));
    ::morphuntion::util::Validate::notNull(dictionary.getBinaryProperties(&backUnroundProperty, {u"back-unround"}));
    ::morphuntion::util::Validate::notNull(dictionary.getBinaryProperties(&foreignProperty, {u"foreign"}));
    ::morphuntion::util::Validate::notNull(dictionary.getBinaryProperties(&exceptionProperty, {u"exception"}));
    ::morphuntion::util::Validate::notNull(dictionary.getBinaryProperties(&compoundProperty, {u"compound"}));
    ::morphuntion::util::Validate::notNull(dictionary.getBinaryProperties(&softConsonantProperty, {u"soft-consonant"}));
    ::morphuntion::util::Validate::notNull(dictionary.getBinaryProperties(&hardConsonantProperty, {u"hard-consonant"}));
}

TrGrammarSynthesizer_TrDisplayFunction::~TrGrammarSynthesizer_TrDisplayFunction()
{
}

static constexpr char16_t DEFAULT_DISCONT_HARD_CONS_STR[] = { u"çkptćč" };

const ::icu4cxx::UnicodeSet& TrGrammarSynthesizer_TrDisplayFunction::DEFAULT_DISCONT_HARD_CONSONANTS_END()
{
    static auto DEFAULT_DISCONT_HARD_CONSONANTS_END_ = ::morphuntion::util::UnicodeSetUtils::freeze(
            ::morphuntion::util::UnicodeSetUtils::closeOver(
                    new ::icu4cxx::UnicodeSet(::std::u16string(u"[") + DEFAULT_DISCONT_HARD_CONS_STR + ::std::u16string(u"]")), USET_CASE_INSENSITIVE));
    return *npc(DEFAULT_DISCONT_HARD_CONSONANTS_END_);
}

const ::icu4cxx::UnicodeSet& TrGrammarSynthesizer_TrDisplayFunction::DEFAULT_HARD_CONSONANTS_END()
{
    static auto DEFAULT_HARD_CONSONANTS_END_ = ::morphuntion::util::UnicodeSetUtils::freeze(
            ::morphuntion::util::UnicodeSetUtils::closeOver(
                    new ::icu4cxx::UnicodeSet(::std::u16string(u"[") + DEFAULT_DISCONT_HARD_CONS_STR + ::std::u16string(u"fhsşqxš]")), USET_CASE_INSENSITIVE));
    return *npc(DEFAULT_HARD_CONSONANTS_END_);
}

::morphuntion::dialog::SemanticFeatureModel_DisplayValue * TrGrammarSynthesizer_TrDisplayFunction::getDisplayValue(const dialog::SemanticFeatureModel_DisplayData &displayData, const ::std::map<::morphuntion::dialog::SemanticFeature, ::std::u16string> &constraints, bool /*enableInflectionGuess*/) const
{
    ::std::u16string displayString;
    const auto& formConstraints(constraints);
    if (displayData.getValues().empty()) {
        return new ::morphuntion::dialog::SemanticFeatureModel_DisplayValue(displayString, formConstraints);
    } else {
        displayString = displayData.getValues()[0].getDisplayString();
    }
    if (displayString.empty()) {
        return new ::morphuntion::dialog::SemanticFeatureModel_DisplayValue(displayString, formConstraints);
    }
    auto displayStringNormalized = getLastWord(displayString);
    if (!isForeignWord(displayStringNormalized)) {
        auto trimmedStr = trimEnd(displayStringNormalized);
        if (displayStringNormalized.length() != trimmedStr.length() && isForeignWord(trimmedStr)) {
            displayStringNormalized = trimmedStr;
        } else {
            displayStringNormalized = normalizeString(displayStringNormalized);
        }
    }
    auto caseValue = TrGrammarSynthesizer::getCase(::morphuntion::dialog::DefaultArticleLookupFunction::getFeatureValue(constraints, caseFeature));
    auto countValue = TrGrammarSynthesizer::getCount(::morphuntion::dialog::DefaultArticleLookupFunction::getFeatureValue(constraints, countFeature));
    auto personValue = TrGrammarSynthesizer::getPerson(::morphuntion::dialog::DefaultArticleLookupFunction::getFeatureValue(constraints, personFeature));
    auto copulaValue = TrGrammarSynthesizer::getCopula(::morphuntion::dialog::DefaultArticleLookupFunction::getFeatureValue(constraints, copulaFeature));
    auto tenseValue = TrGrammarSynthesizer::getTense(::morphuntion::dialog::DefaultArticleLookupFunction::getFeatureValue(constraints, tenseFeature));
    auto pronounNumberCategoryValue = ::morphuntion::dialog::DefaultArticleLookupFunction::getFeatureValue(constraints, pronounNumberFeature);
    if (pronounNumberCategoryValue == nullptr) {
        // TODO remove this if statement when the pronoun category is no longer used.
        pronounNumberCategoryValue = ::morphuntion::dialog::DefaultArticleLookupFunction::getFeatureValue(constraints, pronounFeature);
    }
    auto pronounValue = TrGrammarSynthesizer::getPronoun(pronounNumberCategoryValue);
    ::std::u16string suffixString;
    auto endsWithNumber = TrGrammarSynthesizer_TrDisplayFunction::endsWithNumber(displayString);
    auto isCompound = isCompoundWord(displayStringNormalized);
    auto isPossessive = false;
    auto possessiveCompoundSuffix = getPossessiveCompoundSuffix(displayString.find(u' ') != ::std::u16string::npos, displayStringNormalized);
    auto isPossessiveCompound = false;
    if (!isForeignWord(displayStringNormalized)) {
        isPossessiveCompound = isCompound || (!endsWithNumber && !possessiveCompoundSuffix.empty());
        isPossessive = isPossessiveCompound;
        if (isPossessiveCompound && getLastWord(displayString) == displayStringNormalized
                && (countValue == TrGrammarSynthesizer::Count::PLURAL || personValue != TrGrammarSynthesizer::Person::undefined || pronounValue != TrGrammarSynthesizer::Pronoun::undefined))
        {
            displayStringNormalized.resize(displayStringNormalized.length() - possessiveCompoundSuffix.length());
            displayString.resize(displayString.length() - possessiveCompoundSuffix.length());
            auto displayStringLemmatized = revertLastConsonantAssimilation(displayStringNormalized);
            if (displayStringNormalized != displayStringLemmatized) {
                displayStringNormalized = displayStringLemmatized;
                displayString = revertLastConsonantAssimilation(displayString);
            }
        }
    }
    auto vowGroup = getVowelGroup(displayStringNormalized);
    auto isForeign = isForeignWord(displayStringNormalized);
    auto isException = isExceptionWord(displayStringNormalized);
    auto lastPart = displayStringNormalized;
    if (countValue == TrGrammarSynthesizer::Count::PLURAL) {
        if (vowGroup == TrGrammarSynthesizer::VowelGroup::BACK_UNROUNDED || vowGroup == TrGrammarSynthesizer::VowelGroup::BACK_ROUNDED) {
            suffixString += u"lar";
            vowGroup = TrGrammarSynthesizer::VowelGroup::BACK_UNROUNDED;
            if (isPossessiveCompound) {
                suffixString += u"ı";
            }
        } else {
            suffixString += u"ler";
            vowGroup = TrGrammarSynthesizer::VowelGroup::FRONT_UNROUNDED;
            if (isPossessiveCompound) {
                suffixString += u"i";
            }
        }
        lastPart = suffixString;
    }
    if (personValue != TrGrammarSynthesizer::Person::undefined || pronounValue != TrGrammarSynthesizer::Pronoun::undefined) {
        lastPart = addPossessiveSuffixes(countValue, personValue, pronounValue, isPossessiveCompound, vowGroup, lastPart);
        suffixString += lastPart;
        isPossessive = true;
    }

    lastPart = addCaseSuffixes(caseValue, isPossessive, vowGroup, lastPart);
    suffixString += lastPart;

    if (lastPart.empty() && suffixString.empty()){
        suffixString += addCopulaSuffixes(copulaValue, tenseValue, vowGroup, displayStringNormalized);
    }else if(lastPart.empty()){
        suffixString += addCopulaSuffixes(copulaValue, tenseValue, vowGroup, suffixString);
    }else{
        suffixString += addCopulaSuffixes(copulaValue, tenseValue, vowGroup, lastPart);
    }

    return generateDisplayValue(displayString, formConstraints, displayStringNormalized, &suffixString, endsWithNumber, isForeign, isException);
}

// The sizes are Case, isPossessive and size
static const char16_t POSSESSIVE[7][2][2] = {
        {u"", u""}, // undefined
        {u"n", u""}, // ablative
        {u"n", u"y"}, // accusative
        {u"n", u"y"}, // dative
        {u"n", u"n"}, // genitive
        {u"n", u""},  // locative
        {u"y", u"y"}  // instrumental
};
// The sizes are Case, isPossessive and size
static const char16_t ROUNDED[7][2][4] = {
        {u"", u""}, // undefined
        {u"den", u"dan"}, // ablative
        {u"", u""}, // accusative
        {u"e", u"a"}, // dative
        {u"n", u"n"}, // genitive
        {u"de", u"da"},  // locative
        {u"le", u"la"}  // instrumental
};

std::u16string TrGrammarSynthesizer_TrDisplayFunction::addCaseSuffixes(
        TrGrammarSynthesizer::Case caseValue,
        bool isPossessive,
        TrGrammarSynthesizer::VowelGroup &vowGroup,
        const std::u16string &lastPart)
{
    std::u16string result;
    if (caseValue != TrGrammarSynthesizer::Case::undefined) {
        if (TrGrammarSynthesizer_TrDisplayFunction::endsWithVowel(lastPart)) {
            result += POSSESSIVE[(int32_t)caseValue][isPossessive == 0];
        }

        auto front = (vowGroup == TrGrammarSynthesizer::VowelGroup::FRONT_UNROUNDED) ||
                     (vowGroup == TrGrammarSynthesizer::VowelGroup::FRONT_ROUNDED);

        if (caseValue == TrGrammarSynthesizer::Case::ACCUSATIVE ||
            caseValue == TrGrammarSynthesizer::Case::GENITIVE)
        {
            result += TrGrammarSynthesizer::getAffix(vowGroup);
        }
        else if (caseValue == TrGrammarSynthesizer::Case::DATIVE ||
                 caseValue == TrGrammarSynthesizer::Case::LOCATIVE ||
                 caseValue == TrGrammarSynthesizer::Case::ABLATIVE ||
                 caseValue == TrGrammarSynthesizer::Case::INSTRUMENTAL )
        {
            if (front) {
                vowGroup = TrGrammarSynthesizer::VowelGroup::FRONT_UNROUNDED;
            }
            else {
                vowGroup = TrGrammarSynthesizer::VowelGroup::BACK_UNROUNDED;
            }
        }

        result.append(ROUNDED[(int32_t) caseValue][front == 0]);
    }
    return result;
}

std::u16string TrGrammarSynthesizer_TrDisplayFunction::addPossessiveSuffixes(TrGrammarSynthesizer::Count countValue, TrGrammarSynthesizer::Person personValue, TrGrammarSynthesizer::Pronoun pronounValue, bool isPossessiveCompound, TrGrammarSynthesizer::VowelGroup& vowGroup, const std::u16string& lastPart)
{
    std::u16string result;
    if (pronounValue == TrGrammarSynthesizer::Pronoun::undefined) {
        pronounValue = TrGrammarSynthesizer::Pronoun::SINGULAR;
    }

    if (personValue == TrGrammarSynthesizer::Person::THIRD && pronounValue == TrGrammarSynthesizer::Pronoun::SINGULAR) {
        if (endsWithVowel(lastPart)) {
            result += u's';
        }
        result += TrGrammarSynthesizer::getAffix(vowGroup);
    } else if (personValue == TrGrammarSynthesizer::Person::THIRD && pronounValue == TrGrammarSynthesizer::Pronoun::PLURAL) {
        if (countValue == TrGrammarSynthesizer::Count::PLURAL) {
            if (!isPossessiveCompound) {
                if (vowGroup == TrGrammarSynthesizer::VowelGroup::BACK_UNROUNDED) {
                    result += u'\u0131';
                    vowGroup = TrGrammarSynthesizer::VowelGroup::BACK_UNROUNDED;
                } else {
                    result += u'i';
                    vowGroup = TrGrammarSynthesizer::VowelGroup::FRONT_UNROUNDED;
                }
            }
        } else {
            if (vowGroup == TrGrammarSynthesizer::VowelGroup::BACK_UNROUNDED || vowGroup == TrGrammarSynthesizer::VowelGroup::BACK_ROUNDED) {
                result += u"ları";
                vowGroup = TrGrammarSynthesizer::VowelGroup::BACK_UNROUNDED;
            } else {
                result += u"leri";
                vowGroup = TrGrammarSynthesizer::VowelGroup::FRONT_UNROUNDED;
            }
        }
    } else {
        if (!endsWithVowel(lastPart)) {
            result += TrGrammarSynthesizer::getAffix(vowGroup);
        }
        if (personValue == TrGrammarSynthesizer::Person::FIRST) {
            result += u'm';
        } else { // Person::SECOND or Person::undefined
            result += u'n';
        }
        if (pronounValue == TrGrammarSynthesizer::Pronoun::PLURAL) {
            result.append(1, TrGrammarSynthesizer::getAffix(vowGroup)).append(1, u'z');
        }
    }
    return result;
}

// The sizes are Tense, Copula and size
static const char16_t BEFORE_CONSONANT_GROUP[3][4][2] = {
        {u"", u"", u"", u""}, // undefined
        {u"", u"y", u"", u""}, // present
        {u"", u"y", u"y", u"y"} // past
};
// The sizes are Tense, Copula and size
static const char16_t BEFORE_VOWEL_GROUP[3][4][2] = {
        {u"", u"", u"", u""}, // undefined
        {u"", u"", u"s", u"d"}, // present
        {u"", u"d", u"d", u"d"} // past
};
// The sizes are Tense, Copula and size
static const char16_t AFTER_VOWEL_GROUP[3][4][2] = {
        {u"", u"", u"", u""}, // undefined
        {u"", u"m", u"n", u"r"}, // present
        {u"", u"m", u"n", u""} // past
};

::std::u16string TrGrammarSynthesizer_TrDisplayFunction::addCopulaSuffixes(
        TrGrammarSynthesizer::Copula copulaValue,
        TrGrammarSynthesizer::Tense tenseValue,
        TrGrammarSynthesizer::VowelGroup vowGroup,
        const std::u16string &lastPart)
{
    std::u16string result;
    if (copulaValue != TrGrammarSynthesizer::Copula::undefined || tenseValue != TrGrammarSynthesizer::Tense::undefined) {
        if (copulaValue == TrGrammarSynthesizer::Copula::undefined) {
            copulaValue = TrGrammarSynthesizer::Copula::THIRD;
        } else if (tenseValue == TrGrammarSynthesizer::Tense::undefined) {
            tenseValue = TrGrammarSynthesizer::Tense::present;
        }

        if (TrGrammarSynthesizer_TrDisplayFunction::endsWithVowel(lastPart)) {
            result.append(BEFORE_CONSONANT_GROUP[(int32_t) tenseValue][(int32_t) copulaValue]);
        }

        result
                .append(BEFORE_VOWEL_GROUP[(int32_t) tenseValue][(int32_t) copulaValue])
                .append(1, TrGrammarSynthesizer::getAffix(vowGroup))
                .append(AFTER_VOWEL_GROUP[(int32_t) tenseValue][(int32_t) copulaValue]);
    }
    return result;
}

morphuntion::dialog::SemanticFeatureModel_DisplayValue* TrGrammarSynthesizer_TrDisplayFunction::generateDisplayValue(const ::std::u16string& displayStr, const ::std::map<::morphuntion::dialog::SemanticFeature, ::std::u16string>& formConstraints, const std::u16string& displayStringNormalized, std::u16string* suffixString, bool endsWithNumber, bool isForeign, bool isException) const
{
    std::u16string displayString(displayStr);
    if (!displayString.empty()) {
        if (getLastWord(displayString) == displayStringNormalized && !isForeign && !isException) {
            if (!npc(suffixString)->empty() && startsWithVowel(*npc(suffixString)) && endsWithHardConsonant(displayString, true) && !isOneVowelWord(displayStringNormalized)) {
                displayString = assimilateLastConsonant(displayString, false);
            }
        }
        else if (isException && dictionary.hasAllProperties(displayStringNormalized, hardConsonantProperty) && !npc(suffixString)->empty() && startsWithVowel(*npc(suffixString))) {
            displayString = assimilateLastConsonant(displayString, isOneVowelWord(displayStringNormalized));
        }
        if (!npc(suffixString)->empty() && doesEndWithHardConsonant(displayStringNormalized)) {
            *npc(suffixString) = assimilateFirstConsonant(*npc(suffixString));
        }
    }
    if (!npc(suffixString)->empty()
        && ((isForeign || endsWithNumber) && !displayString.empty() && !(TrGrammarSynthesizer::ENDING_DOUBLE_QUOTE().contains(displayString.back()))))
    {
        if (TrGrammarSynthesizer::ENDING_SINGLE_QUOTE().contains(displayString.back())) {
            displayString = displayString.substr(0, displayString.length() - 1);
        }
        return new morphuntion::dialog::SemanticFeatureModel_DisplayValue(displayString + u"’" + *npc(suffixString), formConstraints);
    }
    return new morphuntion::dialog::SemanticFeatureModel_DisplayValue(displayString + *npc(suffixString), formConstraints);
}

::std::u16string TrGrammarSynthesizer_TrDisplayFunction::normalizeString(const ::std::u16string& originalWord) const
{
    ::std::u16string word(originalWord);
    if (word.empty()) {
        return {};
    }
    auto endsWithPeriod = false;
    if (word.back() == u'.') {
        endsWithPeriod = true;
    }
    word = trimEnd(word);
    if (!word.empty() && !isForeignWord(word) && (bool)u_isdigit(word.back())) {
        //split string based on (:, / and space)
        int32_t lastIndex = int32_t(word.length());
        double lastNumber = -DBL_MAX;
        auto status = U_ZERO_ERROR;
        ::icu4cxx::NumberFormat numberFormat(unum_open(UNUM_DECIMAL, nullptr, -1, ::morphuntion::util::LocaleUtils::TURKISH().getName().c_str(), nullptr, &status));

        if (U_SUCCESS(status)) {
            while (lastNumber <= 0 && lastIndex > 0) {
                auto firstIndex = TrGrammarSynthesizer::SEPARATOR_SPLITTER().spanBack(word.substr(0, lastIndex), USET_SPAN_NOT_CONTAINED);
                lastNumber = unum_parseDouble(numberFormat.wrappee_, (const UChar *)word.c_str() + firstIndex, lastIndex - firstIndex, nullptr, &status);
                if (U_FAILURE(status)) {
                    status = U_ZERO_ERROR;
                    if (word[lastIndex - 1] != u'0') {
                        lastNumber = unum_parseDouble(numberFormat.wrappee_, (const UChar *)word.c_str() + lastIndex - 1, 1, nullptr, &status);
                        if (U_FAILURE(status)) {
                            status = U_ZERO_ERROR;
                        }
                    }
                    break;
                }
                lastIndex = firstIndex - 1;
            }
        }

        if (lastNumber == -DBL_MAX) {
            return word;
        }

        ::morphuntion::dialog::NumberConcept number(lastNumber, ::morphuntion::util::LocaleUtils::TURKISH(), ::morphuntion::util::LocaleUtils::TURKISH());
        if (endsWithPeriod) {
            return number.asWords(u"ordinal");
        }
        else {
            return number.getAsWords();
        }
    }
    return word;
}

char16_t TrGrammarSynthesizer_TrDisplayFunction::getLastLetter(const ::std::u16string& word) const
{
    auto wordNorm(normalizeString(word));
    if (wordNorm.empty()) {
        return 0;
    }
    return wordNorm.back();
}

bool TrGrammarSynthesizer_TrDisplayFunction::endsWithVowel(const ::std::u16string& word)
{
    return ::morphuntion::dictionary::PhraseProperties::isEndsWithVowel(::morphuntion::util::LocaleUtils::TURKISH(), word);
}

bool TrGrammarSynthesizer_TrDisplayFunction::startsWithVowel(const ::std::u16string& word)
{
    return ::morphuntion::dictionary::PhraseProperties::isStartsWithVowel(::morphuntion::util::LocaleUtils::TURKISH(), word);
}

::std::u16string TrGrammarSynthesizer_TrDisplayFunction::getLastWord(const ::std::u16string& str)
{
    std::u16string string(::morphuntion::util::StringViewUtils::trim(str));
    auto lastIndex = string.rfind(u' ');
    if (lastIndex == ::std::u16string::npos) {
        return string;
    }
    return string.substr(lastIndex + 1);
}

bool TrGrammarSynthesizer_TrDisplayFunction::endsWithHardConsonant(::std::u16string_view word, bool discontinuous)
{
    if (word.length() < 2) {
        if (word.empty()) {
            return false;
        }
        auto onlyChar = word[0];
        return (onlyChar == u'\u00e7' || onlyChar == u'k' || onlyChar == u'p' || onlyChar == u't')
            || (!discontinuous && (onlyChar == u'f' || onlyChar == u'h' || onlyChar == u's' || onlyChar == u'\u015f' || onlyChar == u'q' || onlyChar == u'x'));
    }
    if (discontinuous) {
        return DEFAULT_DISCONT_HARD_CONSONANTS_END().contains(word.back());
    } else {
        return DEFAULT_HARD_CONSONANTS_END().contains(word.back());
    }
}

static const char16_t POSSESSIVE_COMPOUND_SUFFIXES[][5] = {
    u"ları"
    , u"leri"
    , u"sı"
    , u"si"
    , u"su"
    , u"sü"
    , u"ı"
    , u"i"
    , u"u"
    , u"ü"
};

::std::u16string TrGrammarSynthesizer_TrDisplayFunction::getPossessiveCompoundSuffix(bool isDisplayMultiWord, const ::std::u16string& word) const
{
    if (isDisplayMultiWord) {
        for (const auto possessiveSuffixItem : POSSESSIVE_COMPOUND_SUFFIXES) {
            ::std::u16string possessiveSuffix(possessiveSuffixItem);
            if (::morphuntion::util::StringViewUtils::endsWith(word, possessiveSuffix)) {
                ::std::u16string lastWordTrimmed(word, 0, word.length() - possessiveSuffix.length());
                if (endsWithNumber(lastWordTrimmed)) {
                    break;
                }
                if ((isOneToken(lastWordTrimmed) || isExceptionWord(lastWordTrimmed) || isCompoundWord(lastWordTrimmed))
                    && (dictionary.isKnownWord(lastWordTrimmed) && (!isExceptionWord(word)) && (possessiveSuffix.length() > 2 || !isOneToken(word))))
                {
                    return possessiveSuffix;
                }
                auto lastWordLemmatized = revertLastConsonantAssimilation(lastWordTrimmed);
                if (lastWordLemmatized != lastWordTrimmed
                    && (isOneToken(lastWordLemmatized) || isExceptionWord(lastWordLemmatized) || isCompoundWord(lastWordLemmatized))
                    && (dictionary.isKnownWord(lastWordLemmatized) && (possessiveSuffix.length() > 2 || !isOneToken(word))))
                {
                    return possessiveSuffix;
                }
            }
        }
    }
    return {};
}

bool TrGrammarSynthesizer_TrDisplayFunction::isOneToken(const ::std::u16string& word) const
{
    ::std::unique_ptr<::morphuntion::tokenizer::TokenChain> tokens(npc(npc(tokenizer.get())->createTokenChain(word)));
    return tokens->getWordCount() == 1;
}

char16_t TrGrammarSynthesizer_TrDisplayFunction::getLastVowel(const ::std::u16string& word) const
{
    if (endsWithVowel(word)) {
        return getLastLetter(word);
    }
    ::std::u16string stem(word);
    while (!stem.empty()) {
        stem.resize(stem.length() - 1);
        if (::morphuntion::dictionary::PhraseProperties::isEndsWithUnicodeSets(stem, ::morphuntion::dictionary::PhraseProperties::DEFAULT_MATCHABLE_SET(), ::morphuntion::dictionary::PhraseProperties::DEFAULT_VOWELS_START(), {})) {
            return getLastLetter(stem);
        }
    }
    return u'i';
}

::std::u16string TrGrammarSynthesizer_TrDisplayFunction::assimilateLastConsonant(const ::std::u16string& word, bool oneWord)
{
    if (!word.empty()) {
        switch (word.back()) {
            case u'\u00e7':
                return word.substr(0, word.length() - 1) + u"c";
            case u'k':
            case u'g':
                if (!oneWord) {
                    return word.substr(0, word.length() - 1) + u"ğ";
                }
                else {
                    return word.substr(0, word.length() - 1) + u"g";
                }
            case u'p':
                return word.substr(0, word.length() - 1) + u"b";
            case u't':
                return word.substr(0, word.length() - 1) + u"d";
            default:
                break;
        }
    }

    return word;
}

::std::u16string TrGrammarSynthesizer_TrDisplayFunction::revertLastConsonantAssimilation(const ::std::u16string& word)
{
    if (!word.empty()) {
        switch (word.back()) {
            case u'b':
                return word.substr(0, word.length() - 1) + u"p";
            case u'c':
                return word.substr(0, word.length() - 1) + u"ç";
            case u'd':
                return word.substr(0, word.length() - 1) + u"t";
            case u'\u011f':
                return word.substr(0, word.length() - 1) + u"k";
            default:
                break;
        }
    }

    return word;
}

::std::u16string TrGrammarSynthesizer_TrDisplayFunction::assimilateFirstConsonant(const ::std::u16string& word)
{
    if (!word.empty()) {
        switch (word[0]) {
            case u'b':
                return u"p" + word.substr(1);
            case u'c':
                return u"ç" + word.substr(1);
            case u'd':
                return u"t" + word.substr(1);
            case u'g':
                return u"k" + word.substr(1);
            default:
                break;
        }
    }

    return word;
}

TrGrammarSynthesizer::VowelGroup TrGrammarSynthesizer_TrDisplayFunction::getVowelGroup(::std::u16string_view str) const
{
    int64_t properties = 0;
    if (dictionary.getCombinedBinaryType(&properties, str) != nullptr) {
        if ((properties & frontRoundProperty) != 0) {
            return TrGrammarSynthesizer::VowelGroup::FRONT_ROUNDED;
        } else if ((properties & frontUnroundProperty) != 0) {
            return TrGrammarSynthesizer::VowelGroup::FRONT_UNROUNDED;
        } else if ((properties & backRoundProperty) != 0) {
            return TrGrammarSynthesizer::VowelGroup::BACK_ROUNDED;
        } else if ((properties & backUnroundProperty) != 0) {
            return TrGrammarSynthesizer::VowelGroup::BACK_UNROUNDED;
        }
    }
    ::std::u16string lowerStr;
    ::morphuntion::util::StringViewUtils::lowercase(&lowerStr, str, ::morphuntion::util::LocaleUtils::TURKISH());
    switch (getLastVowel(lowerStr)) {
        case u'e':
        case u'i':
        case u'\u00e4':
        case u'\u00e9':
        case u'\u00e8':
        case u'\u00eb':
        case u'\u011b':
        case u'\u00ed':
        case u'\u00ef':
        case u'\u00fd':
            return TrGrammarSynthesizer::VowelGroup::FRONT_UNROUNDED;
        case u'\u00f6':
        case u'\u00fc':
        case u'\u00f8':
            return TrGrammarSynthesizer::VowelGroup::FRONT_ROUNDED;
        case u'a':
        case u'\u0131':
        case u'\u0103':
        case u'\u00e1':
        case u'\u00e3':
            return TrGrammarSynthesizer::VowelGroup::BACK_UNROUNDED;
        case u'o':
        case u'u':
        case u'\u00f3':
        case u'\u00f4':
        case u'\u00fa':
        case u'\u016f':
        case u'\u016b':
            return TrGrammarSynthesizer::VowelGroup::BACK_ROUNDED;
        default:
            break;
    }

    return TrGrammarSynthesizer::VowelGroup::FRONT_UNROUNDED;
}

bool TrGrammarSynthesizer_TrDisplayFunction::isForeignWord(::std::u16string_view str) const
{
    return dictionary.hasAllProperties(str, foreignProperty);
}

bool TrGrammarSynthesizer_TrDisplayFunction::isExceptionWord(::std::u16string_view str) const
{
    return dictionary.hasAllProperties(str, exceptionProperty);
}

bool TrGrammarSynthesizer_TrDisplayFunction::isCompoundWord(::std::u16string_view str) const
{
    return dictionary.hasAllProperties(str, compoundProperty);
}

bool TrGrammarSynthesizer_TrDisplayFunction::doesEndWithSoftConsonant(::std::u16string_view str) const
{
    return dictionary.hasAllProperties(str, softConsonantProperty);
}

bool TrGrammarSynthesizer_TrDisplayFunction::doesEndWithHardConsonant(::std::u16string_view str) const
{
    if (doesEndWithSoftConsonant(str)) {
        return false;
    }
    return (endsWithHardConsonant(str, false) || dictionary.hasAllProperties(str, hardConsonantProperty));
}

bool TrGrammarSynthesizer_TrDisplayFunction::isOneVowelWord(::std::u16string_view originalWord)
{
    int32_t vowelCount = 0;
    ::std::u16string word(originalWord);
    while (!word.empty()) {
        if (endsWithVowel(word)) {
            vowelCount++;
        }
        word.resize(word.length() - 1);
    }
    return (vowelCount <= 1);
}

bool TrGrammarSynthesizer_TrDisplayFunction::endsWithNumber(const ::std::u16string& originalWord)
{
    ::std::u16string word(trimEnd(originalWord));
    if (word.empty()) {
        return false;
    }
    return u_isdigit(word.back());
}

::std::u16string TrGrammarSynthesizer_TrDisplayFunction::trimEnd(const ::std::u16string& originalWord)
{
    auto word(originalWord);
    while (!word.empty() && !::morphuntion::dictionary::PhraseProperties::DEFAULT_MATCHABLE_SET().contains(::morphuntion::util::StringViewUtils::codePointAt(word, int32_t(word.length() - 1)))) {
        word.resize(word.length() - 1);
    }
    return word;
}

} // namespace morphuntion::grammar::synthesis
