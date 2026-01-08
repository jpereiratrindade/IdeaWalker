/**
 * @file CognitiveModel.hpp
 * @brief Defines the cognitive types and structures for the AI pipeline.
 */

#pragma once
#include <string>

namespace ideawalker::domain {

/**
 * @enum AIPersona
 * @brief Defines the personality/role of the AI.
 */
enum class AIPersona {
    AnalistaCognitivo,   ///< Deep, strategic, tension-focused.
    SecretarioExecutivo, ///< Concise, task-focused, summary.
    Brainstormer,        ///< Expansive, creative, divergent.
    Orquestrador,        ///< Meta-persona: diagnoses and sequences other personas.
    Tecelao              ///< Connective: bridges notes and identifies emergent relationships.
};

/**
 * @enum CognitiveState
 * @brief Represents the diagnosed state of the text/thought.
 */
enum class CognitiveState {
    Unknown,
    Chaotic,     ///< Disorganized, raw, stream of consciousness.
    Divergent,   ///< Many ideas, branching, potential overwhelm.
    Convergent,  ///< Focusing, structuring, patterns emerging.
    Integrative, ///< Connecting concepts, synthesis.
    Closing      ///< Defining actions, final polish.
};

/**
 * @struct CognitiveSnapshot
 * @brief Records a single transformation step in the cognitive pipeline.
 */
struct CognitiveSnapshot {
    AIPersona persona;
    CognitiveState state;
    std::string textInput;
    std::string textOutput;
    std::string reasoning; // Why this transformation?
    std::string timestamp;
};

} // namespace ideawalker::domain
