/**
 * @file FactorySets.h
 * @brief The factory's DJ sets (17.10.2026): 256 sets to begin a night with, each a `.totset` with a name, the style it
 *        starts in, its arc and its length.
 *
 * A factory set is what a player would save after an evening of curating: a seed and the set's knobs -- the style, the
 * arc (set.dramaturgy), whether the styles wander, the set's length, a track's time, the blend, the loops, the FX
 * breaks, the DJ's hand, the pull towards Dub or Hypnotic. Everything else stays at its default, so the composer picks
 * every track's sounds as always. Loaded in the plugin (the Sets browser), listed by the render app (Totality Sets) and
 * by Heliacal. Tools/sets/make_sets.py draws them (FactorySetsData.inc) and writes in how long each plays, measured
 * by the render app (its --catalog composes every one); every eighth is composed again in the self-test.
 * @author Rene Weller
 */
#pragma once

namespace tot
{

/** @brief A set of the factory. */
struct FactorySet
{
    const char *m_name;      ///< its name
    const char *m_section;   ///< the style it starts in (StyleNames)
    const char *m_group;     ///< its arc (DramaturgyNames)
    const char *m_note;      ///< a few words: its length, whether its styles wander
    const char *m_text;      ///< the `.totset` (loadSetText)
    double      m_seconds;   ///< how long it plays, as composed when the bank was made (0: not measured)
};

/**
 * @brief How many sets the factory has.
 * @return the count
 */
int numFactorySets();

/**
 * @brief A set of the factory.
 * @param[in] index  0 .. numFactorySets() - 1 (clamped)
 * @return the set
 */
const FactorySet &factorySet( int index );

/**
 * @brief A set of the factory by its name.
 * @param[in] name  the name
 * @return its index, -1 when there is none of that name
 */
int findFactorySet( const char *name );

}   // namespace tot
