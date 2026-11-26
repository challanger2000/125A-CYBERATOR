#pragma once
#include "Parameters.hpp"
#include <initializer_list>
namespace cyberator {
enum class PresetCategory : unsigned { cyberVocals, ghostVoices, granularAtmospheres, rhythmic, darkIndustrial, ambientShimmer, extremeExperimental };
struct FactoryPreset {
    std::string_view name;
    PresetCategory category;
    std::array<float,Parameters::count> values;
    [[nodiscard]] Parameters parameters() const noexcept {
        Parameters p;
        for(std::size_t i=0;i<values.size();++i)(void)p.set(static_cast<Param>(i),values[i]);
        return p;
    }
};
namespace factory_detail {
using P=Param;using M=Mode;using K=PresetCategory;
struct Assignment { P id; float value; };
inline constexpr auto G=P::granular,Z=P::size,D=P::density,X=P::position,J=P::scatter,T=P::pitch,R=P::reverse,B=P::feedback,W=P::spread,F=P::formant,A=P::android,S=P::spectral,H=P::harmony,E=P::ghost,U=P::swell,L=P::delay,V=P::space,O=P::motion,Q=P::sync,Mix=P::mix,Out=P::output;
consteval FactoryPreset make(std::string_view name,K category,M mode,std::initializer_list<Assignment> changes){
    FactoryPreset p{name,category,{}};std::array<bool,Parameters::count> used{};
    for(std::size_t i=0;i<p.values.size();++i)p.values[i]=specs[i].initial;
    p.values[static_cast<std::size_t>(P::mode)]=float(mode);
    for(const auto a:changes){const auto i=static_cast<std::size_t>(a.id);
        if(i>=p.values.size()||used[i]||a.id==P::mode||a.id==P::freeze||!(a.value>=specs[i].low&&a.value<=specs[i].high))throw "invalid factory preset assignment";
        if(specs[i].discrete&&a.value!=0&&a.value!=1)throw "invalid discrete value";
        used[i]=true;p.values[i]=a.value;
    }
    return p;
}
#define CYB_CAT(fn,category) consteval FactoryPreset fn(std::string_view n,M m,std::initializer_list<Assignment> v){return make(n,K::category,m,v);}
CYB_CAT(cv,cyberVocals) CYB_CAT(gv,ghostVoices) CYB_CAT(ga,granularAtmospheres) CYB_CAT(rh,rhythmic) CYB_CAT(di,darkIndustrial) CYB_CAT(as,ambientShimmer) CYB_CAT(ee,extremeExperimental)
#undef CYB_CAT
// Individually authored starting points; listening and loudness matching precede release.
inline constexpr std::array<FactoryPreset,120> bank{{
    cv("Human to Machine",M::cyber,{{F,-3},{A,.32f},{S,.14f},{H,.10f},{V,.12f},{Mix,.72f},{Out,-2}}),
    cv("Chrome Whisper",M::cyber,{{F,3},{A,.18f},{S,.26f},{E,.10f},{V,.22f},{Mix,.64f},{Out,-3}}),
    cv("Neon Announcer",M::android,{{F,-2},{A,.52f},{S,.08f},{L,.12f},{Mix,.76f},{Out,-2}}),
    cv("Velvet Android",M::dream,{{F,2},{A,.22f},{S,.31f},{H,.16f},{V,.21f},{Mix,.68f},{Out,-3}}),
    cv("Synthetic Siren",M::shimmer,{{F,5},{A,.21f},{S,.25f},{H,.33f},{U,.12f},{V,.24f},{Mix,.62f},{Out,-4}}),
    cv("Cold Transmission",M::cyber,{{F,-5},{A,.37f},{S,.19f},{L,.16f},{Mix,.81f},{Out,-3}}),
    cv("Silicon Choir",M::shimmer,{{F,1},{A,.12f},{S,.22f},{H,.48f},{V,.31f},{Mix,.67f},{Out,-5}}),
    cv("Digital Intimacy",M::dream,{{F,1},{A,.12f},{S,.10f},{H,.07f},{V,.09f},{Mix,.49f},{Out,-1}}),
    cv("Alien Frequency",M::phantom,{{F,7},{A,.28f},{S,.28f},{H,.19f},{E,.10f},{Mix,.63f},{Out,-4}}),
    cv("Carbon Voice",M::android,{{F,-7},{A,.28f},{S,.17f},{H,.14f},{Mix,.73f},{Out,-3}}),
    cv("Midnight Operator",M::cyber,{{F,-2},{A,.47f},{S,.21f},{E,.13f},{L,.10f},{Mix,.69f},{Out,-3}}),
    cv("Electric Breath",M::drift,{{F,4},{A,.10f},{S,.42f},{U,.11f},{V,.20f},{Mix,.59f},{Out,-3}}),
    cv("Machine Prayer",M::shimmer,{{F,-1},{A,.30f},{H,.30f},{U,.15f},{V,.34f},{Mix,.74f},{Out,-5}}),
    cv("Cybernetic Doubler",M::cyber,{{H,.37f},{A,.14f},{S,.08f},{Mix,.55f},{Out,-3}}),
    cv("Liquid Persona",M::dream,{{F,6},{A,.11f},{S,.34f},{L,.13f},{V,.15f},{Mix,.57f},{Out,-3}}),
    cv("Satellite Dialogue",M::pulse,{{F,-4},{A,.24f},{S,.15f},{L,.27f},{Mix,.68f},{Out,-3}}),
    cv("Afterlife Interface",M::phantom,{{F,2},{A,.35f},{S,.23f},{U,.18f},{H,.20f},{Mix,.65f},{Out,-4}}),
    cv("Biomechanical Speech",M::android,{{F,-3},{A,.57f},{H,.17f},{S,.12f},{Mix,.83f},{Out,-4}}),
    cv("Crystal Language",M::shimmer,{{F,4},{A,.08f},{S,.37f},{H,.22f},{V,.18f},{Mix,.71f},{Out,-4}}),
    cv("Future Broadcast",M::cyber,{{F,-1},{A,.39f},{S,.11f},{H,.08f},{L,.18f},{Mix,.78f},{Out,-3}}),
    gv("Before the Voice",M::ghost,{{U,.46f},{V,.13f},{Mix,.80f},{Out,-3}}),
    gv("Reverse Apparition",M::ghost,{{E,.52f},{U,.20f},{V,.19f},{Mix,.78f},{Out,-4}}),
    gv("Whispers Behind Glass",M::phantom,{{E,.35f},{U,.31f},{S,.15f},{V,.25f},{Mix,.70f},{Out,-5}}),
    gv("Haunted Corridor",M::abyss,{{E,.43f},{U,.19f},{L,.16f},{V,.45f},{Mix,.76f},{Out,-6}}),
    gv("Echoes of Tomorrow",M::dream,{{E,.38f},{U,.24f},{L,.26f},{Mix,.64f},{Out,-4}}),
    gv("Disembodied Choir",M::ghost,{{F,3},{H,.24f},{U,.40f},{V,.29f},{Mix,.70f},{Out,-5}}),
    gv("Shadow on the Wire",M::phantom,{{F,-4},{A,.18f},{E,.45f},{U,.18f},{Mix,.70f},{Out,-4}}),
    gv("The Empty Room",M::drift,{{U,.22f},{V,.58f},{S,.21f},{Mix,.74f},{Out,-5}}),
    gv("Approaching Phantom",M::ghost,{{U,.64f},{E,.16f},{Mix,.85f},{Out,-5}}),
    gv("Distant Seance",M::abyss,{{F,-6},{H,.17f},{U,.31f},{E,.23f},{V,.33f},{Mix,.66f},{Out,-5}}),
    gv("Reverse Confession",M::phantom,{{E,.61f},{S,.09f},{L,.12f},{Mix,.82f},{Out,-4}}),
    gv("Astral Twin",M::shimmer,{{H,.35f},{U,.29f},{E,.15f},{V,.23f},{Mix,.65f},{Out,-5}}),
    gv("Ghost in the Machine",M::android,{{A,.35f},{F,-2},{E,.57f},{U,.28f},{Mix,.73f},{Out,-4}}),
    gv("Frozen Breath",M::ghost,{{G,.18f},{Z,90},{D,.35f},{R,1},{J,.17f},{U,.35f},{S,.24f},{V,.20f},{Mix,.68f},{Out,-5}}),
    gv("Backward Horizon",M::drift,{{G,.27f},{Z,170},{D,.43f},{R,1},{W,.5f},{E,.25f},{V,.33f},{O,.2f},{Mix,.75f},{Out,-4}}),
    gv("The Unseen",M::abyss,{{U,.58f},{F,-3},{V,.30f},{Mix,.62f},{Out,-5}}),
    gv("Lost Radio Signal",M::phantom,{{F,4},{A,.21f},{E,.32f},{L,.37f},{S,.15f},{Mix,.78f},{Out,-4}}),
    gv("Spectral Mourning",M::ghost,{{S,.48f},{U,.33f},{H,.14f},{V,.25f},{Mix,.77f},{Out,-5}}),
    gv("Afterimage",M::dream,{{U,.24f},{E,.27f},{H,.12f},{L,.14f},{Mix,.52f},{Out,-3}}),
    gv("Beyond the Door",M::abyss,{{F,-5},{U,.42f},{E,.39f},{V,.46f},{Mix,.81f},{Out,-6}}),
    ga("Glacier Dust",M::drift,{{G,.67f},{Z,220},{D,.47f},{X,.31f},{J,.20f},{T,12},{W,.54f},{V,.34f},{O,.27f},{Mix,.81f},{Out,-4}}),
    ga("Dissolving Words",M::dream,{{G,.50f},{Z,75},{D,.62f},{X,.18f},{J,.31f},{W,.42f},{S,.16f},{Mix,.73f},{Out,-3}}),
    ga("Time Shards",M::shatter,{{G,.72f},{Z,35},{D,.61f},{J,.57f},{T,7},{R,.32f},{W,.70f},{Mix,.78f},{Out,-4}}),
    ga("Cloud of Syllables",M::shimmer,{{G,.64f},{Z,145},{D,.58f},{X,.25f},{J,.36f},{T,12},{W,.58f},{V,.32f},{Mix,.78f},{Out,-4}}),
    ga("Tape Memory",M::drift,{{G,.58f},{Z,180},{D,.35f},{X,.55f},{J,.12f},{T,-5},{B,.28f},{W,.22f},{O,.30f},{Mix,.71f},{Out,-3}}),
    ga("Dust in Reverse",M::ghost,{{G,.61f},{Z,105},{D,.58f},{R,1},{J,.22f},{W,.43f},{U,.14f},{Mix,.78f},{Out,-4}}),
    ga("Orbiting Particles",M::cyber,{{G,.57f},{Z,70},{D,.68f},{X,.4f},{J,.35f},{T,5},{W,.82f},{O,.67f},{Mix,.74f},{Out,-4}}),
    ga("Submerged Memory",M::abyss,{{G,.66f},{Z,210},{D,.42f},{X,.48f},{T,-12},{R,.33f},{B,.34f},{V,.26f},{Mix,.83f},{Out,-4}}),
    ga("Glass Snow",M::shimmer,{{G,.62f},{Z,50},{D,.78f},{T,19},{J,.41f},{W,.74f},{L,.14f},{V,.24f},{Mix,.75f},{Out,-5}}),
    ga("Breathing Fragments",M::dream,{{G,.46f},{Z,90},{D,.44f},{J,.25f},{X,.12f},{O,.73f},{V,.21f},{Mix,.66f},{Out,-3}}),
    ga("Floating Debris",M::drift,{{G,.80f},{Z,245},{D,.36f},{X,.64f},{J,.21f},{T,0},{R,.56f},{W,.65f},{Mix,.87f},{Out,-4}}),
    ga("Micro Mosaic",M::shatter,{{G,.66f},{Z,22},{D,.88f},{J,.43f},{W,.55f},{T,-3},{Mix,.76f},{Out,-3}}),
    ga("Silver Particulate",M::shimmer,{{G,.55f},{Z,115},{D,.56f},{T,7},{W,.78f},{S,.18f},{H,.12f},{V,.28f},{Mix,.71f},{Out,-5}}),
    ga("Slow Orbit",M::drift,{{G,.74f},{Z,200},{D,.31f},{X,.38f},{J,.13f},{O,.85f},{W,.45f},{L,.18f},{Mix,.81f},{Out,-4}}),
    ga("Fragmented Dream",M::dream,{{G,.61f},{Z,130},{D,.65f},{R,.28f},{T,3},{J,.38f},{V,.38f},{Mix,.78f},{Out,-4}}),
    ga("Descent into Dust",M::abyss,{{G,.81f},{Z,155},{D,.56f},{T,-19},{R,.20f},{X,.31f},{B,.48f},{Mix,.87f},{Out,-5}}),
    ga("Hologram Rain",M::cyber,{{G,.63f},{Z,44},{D,.76f},{T,12},{J,.55f},{W,.71f},{A,.13f},{L,.15f},{Mix,.75f},{Out,-4}}),
    ga("Paused Conversation",M::dream,{{G,.85f},{Z,38},{D,.65f},{Q,1},{X,.15f},{W,.12f},{Mix,.85f},{Out,-4}}),
    ga("Particle Echo",M::phantom,{{G,.52f},{Z,105},{D,.55f},{R,.55f},{J,.35f},{E,.23f},{L,.20f},{Mix,.76f},{Out,-4}}),
    ga("Translucent Air",M::shimmer,{{G,.48f},{Z,240},{D,.39f},{T,5},{W,.8f},{V,.45f},{S,.24f},{Mix,.69f},{Out,-5}}),
    rh("Sixteenth Pulse",M::dream,{{G,.78f},{Z,48},{D,.25f},{Q,1},{Mix,.82f},{Out,-3}}),
    rh("Triplet Data",M::dream,{{G,.72f},{Z,70},{D,.375f},{Q,1},{T,7},{J,.2f},{Mix,.79f},{Out,-4}}),
    rh("Reverse Stutter",M::pulse,{{G,.82f},{Z,37},{D,.6f},{Q,1},{R,1},{Mix,.87f},{Out,-3}}),
    rh("Syncopated Fragments",M::shatter,{{G,.64f},{Z,85},{D,.32f},{Q,1},{J,.32f},{R,.26f},{Mix,.77f},{Out,-3}}),
    rh("Machine Gate",M::android,{{G,.71f},{Z,34},{D,.45f},{Q,1},{A,.23f},{Mix,.82f},{Out,-4}}),
    rh("Octave Tick",M::pulse,{{G,.64f},{Z,28},{D,.32f},{Q,1},{T,12},{W,.35f},{Mix,.75f},{Out,-4}}),
    rh("Tempo Locked Scatter",M::dream,{{G,.68f},{Z,65},{D,.75f},{Q,1},{J,.65f},{X,.35f},{W,.58f},{Mix,.77f},{Out,-4}}),
    rh("Binary Echo",M::pulse,{{G,.36f},{Z,66},{D,.44f},{Q,1},{L,.43f},{Mix,.71f},{Out,-4}}),
    rh("Rhythmic Gravity",M::abyss,{{G,.78f},{Z,105},{D,.5f},{Q,1},{T,-12},{B,.24f},{Mix,.80f},{Out,-4}}),
    rh("Broken Packet",M::shatter,{{G,.89f},{Z,20},{D,.55f},{Q,1},{R,.5f},{J,.25f},{W,.5f},{Mix,.87f},{Out,-4}}),
    rh("Clockwork Voice",M::cyber,{{G,.44f},{Z,52},{D,.31f},{Q,1},{A,.36f},{L,.15f},{Mix,.72f},{Out,-3}}),
    rh("Pulsing Clouds",M::shimmer,{{G,.60f},{Z,135},{D,.42f},{Q,1},{T,7},{V,.22f},{W,.52f},{Mix,.77f},{Out,-4}}),
    rh("Repeat and Capture",M::dream,{{G,.94f},{Z,55},{D,.5f},{Q,1},{X,.2f},{Mix,.92f},{Out,-3}}),
    rh("Reverse Grid",M::ghost,{{G,.71f},{Z,85},{D,.7f},{Q,1},{R,1},{E,.19f},{Mix,.81f},{Out,-4}}),
    rh("Digital Pendulum",M::pulse,{{G,.64f},{Z,57},{D,.38f},{Q,1},{O,.84f},{X,.5f},{W,.52f},{Mix,.77f},{Out,-4}}),
    di("Steel Throat",M::android,{{F,-8},{A,.54f},{S,.18f},{H,.23f},{Mix,.83f},{Out,-5}}),
    di("Factory Warning",M::cyber,{{F,-5},{A,.65f},{E,.22f},{L,.19f},{Mix,.85f},{Out,-4}}),
    di("Industrial Abyss",M::abyss,{{F,-9},{S,.46f},{H,.22f},{V,.29f},{U,.17f},{Mix,.78f},{Out,-6}}),
    di("Rust Particles",M::shatter,{{G,.81f},{Z,25},{D,.75f},{T,-12},{J,.45f},{R,.25f},{B,.24f},{Mix,.88f},{Out,-4}}),
    di("Concrete Chambers",M::abyss,{{E,.26f},{V,.48f},{L,.17f},{F,-4},{Mix,.79f},{Out,-6}}),
    di("Machine Chant",M::android,{{F,-4},{A,.68f},{H,.39f},{S,.11f},{Mix,.85f},{Out,-6}}),
    di("Broken Intercom",M::shatter,{{G,.43f},{Z,38},{D,.6f},{J,.38f},{A,.49f},{F,-3},{Mix,.77f},{Out,-4}}),
    di("Subsonic Ritual",M::abyss,{{G,.70f},{Z,190},{D,.43f},{T,-24},{X,.21f},{B,.30f},{S,.31f},{Mix,.82f},{Out,-5}}),
    di("Iron Echo",M::phantom,{{F,-6},{A,.28f},{E,.57f},{L,.24f},{Mix,.81f},{Out,-5}}),
    di("Cold Storage",M::drift,{{G,.58f},{Z,160},{D,.47f},{T,-7},{R,.5f},{V,.33f},{Mix,.77f},{Out,-4}}),
    di("Dystopian Dispatch",M::cyber,{{F,-6},{A,.44f},{S,.20f},{H,.13f},{U,.15f},{Mix,.80f},{Out,-4}}),
    di("Mechanical Swarm",M::shatter,{{G,.75f},{Z,30},{D,.86f},{J,.61f},{T,5},{B,.42f},{W,.68f},{Mix,.85f},{Out,-5}}),
    di("Dark Resonance",M::abyss,{{F,-7},{S,.58f},{H,.18f},{V,.42f},{Mix,.72f},{Out,-6}}),
    di("Hostile Broadcast",M::android,{{F,-3},{A,.82f},{E,.19f},{S,.27f},{Mix,.88f},{Out,-5}}),
    di("Terminal Signal",M::phantom,{{G,.53f},{Z,72},{D,.61f},{T,-5},{R,.7f},{E,.30f},{U,.22f},{Mix,.82f},{Out,-5}}),
    as("Halo of Light",M::shimmer,{{H,.28f},{S,.23f},{V,.45f},{U,.11f},{Mix,.67f},{Out,-5}}),
    as("Skyborne Choir",M::shimmer,{{F,4},{H,.47f},{V,.51f},{Mix,.70f},{Out,-6}}),
    as("Luminous Drift",M::drift,{{G,.54f},{Z,205},{D,.4f},{T,12},{W,.68f},{V,.44f},{O,.29f},{Mix,.78f},{Out,-5}}),
    as("Longing in Space",M::dream,{{L,.32f},{V,.64f},{S,.18f},{Mix,.71f},{Out,-5}}),
    as("Aurora Voice",M::shimmer,{{F,5},{S,.34f},{H,.20f},{V,.32f},{Mix,.65f},{Out,-4}}),
    as("Soft Reverse Halo",M::ghost,{{U,.19f},{E,.15f},{V,.45f},{Mix,.58f},{Out,-4}}),
    as("Endless Horizon",M::drift,{{G,.47f},{Z,230},{D,.32f},{T,7},{J,.22f},{V,.58f},{Mix,.76f},{Out,-5}}),
    as("Dream Transmission",M::dream,{{F,2},{A,.10f},{S,.45f},{L,.19f},{V,.31f},{Mix,.66f},{Out,-4}}),
    as("Falling Stars",M::shimmer,{{G,.56f},{Z,88},{D,.62f},{T,19},{R,.24f},{W,.80f},{V,.36f},{Mix,.73f},{Out,-5}}),
    as("Twilight Reflection",M::phantom,{{H,.23f},{E,.24f},{V,.37f},{L,.18f},{Mix,.68f},{Out,-5}}),
    as("Weightless Speech",M::drift,{{F,2},{S,.28f},{G,.31f},{Z,160},{D,.46f},{O,.27f},{V,.25f},{Mix,.62f},{Out,-4}}),
    as("Dawn Chorus",M::shimmer,{{F,3},{H,.40f},{S,.17f},{L,.14f},{V,.48f},{Mix,.66f},{Out,-6}}),
    as("Orbital Reverb",M::dream,{{V,.80f},{L,.27f},{Mix,.77f},{Out,-5}}),
    as("Crystal Afterglow",M::shimmer,{{U,.18f},{S,.38f},{H,.31f},{V,.42f},{Mix,.75f},{Out,-6}}),
    as("Sleeping Satellite",M::drift,{{G,.41f},{Z,250},{D,.35f},{T,12},{X,.43f},{J,.12f},{S,.24f},{V,.44f},{Mix,.77f},{Out,-5}}),
    ee("Digital Breakdown",M::shatter,{{G,.98f},{Z,18},{D,.93f},{J,.82f},{T,24},{R,.55f},{W,.9f},{B,.42f},{Mix,.96f},{Out,-6}}),
    ee("Alien Possession",M::android,{{F,-12},{A,.91f},{H,.57f},{S,.48f},{Mix,.91f},{Out,-8}}),
    ee("Singularity",M::abyss,{{G,.93f},{Z,225},{D,.6f},{T,-24},{B,.72f},{R,.7f},{V,.52f},{Mix,.95f},{Out,-7}}),
    ee("Spectral Melt",M::drift,{{F,10},{S,.88f},{H,.36f},{U,.39f},{V,.40f},{Mix,.90f},{Out,-7}}),
    ee("Reverse Universe",M::ghost,{{G,.82f},{Z,190},{D,.72f},{R,1},{E,.79f},{U,.65f},{J,.58f},{Mix,.92f},{Out,-8}}),
    ee("Maximum Android",M::android,{{A,1},{F,-10},{S,.63f},{H,.38f},{Mix,1},{Out,-7}}),
    ee("Particle Storm",M::shatter,{{G,.92f},{Z,24},{D,1},{T,12},{R,.48f},{J,1},{W,1},{B,.50f},{Mix,.96f},{Out,-7}}),
    ee("Black Hole Chorus",M::abyss,{{H,.82f},{F,-10},{S,.75f},{U,.40f},{V,.46f},{Mix,.93f},{Out,-9}}),
    ee("Chaotic Orbit",M::pulse,{{G,.88f},{Z,45},{D,.81f},{X,.5f},{J,.64f},{T,-17},{R,.48f},{O,1},{W,.92f},{Mix,.94f},{Out,-6}}),
    ee("Shattered Mirror",M::shatter,{{G,.90f},{Z,30},{D,.78f},{T,19},{R,.78f},{J,.68f},{A,.51f},{E,.31f},{Mix,.95f},{Out,-7}}),
    ee("Temporal Collapse",M::phantom,{{G,.74f},{Z,130},{D,.68f},{R,1},{U,.78f},{E,.56f},{S,.40f},{Mix,.93f},{Out,-8}}),
    ee("Infinite Voices",M::shimmer,{{H,.93f},{S,.67f},{F,8},{V,.70f},{Mix,.88f},{Out,-9}}),
    ee("Data Vortex",M::cyber,{{G,.82f},{Z,32},{D,.92f},{X,.44f},{J,.77f},{T,24},{B,.68f},{A,.61f},{Mix,.94f},{Out,-7}}),
    ee("The Other Side",M::ghost,{{U,1},{E,.83f},{H,.42f},{F,-7},{V,.55f},{Mix,.93f},{Out,-9}}),
    ee("System Overload",M::shatter,{{G,.91f},{Z,15},{D,1},{T,-24},{R,.61f},{J,.94f},{B,.81f},{A,.75f},{S,.64f},{L,.34f},{Mix,1},{Out,-9}}),
}};
} // namespace factory_detail
inline constexpr auto& factoryBank=factory_detail::bank;
} // namespace cyberator
