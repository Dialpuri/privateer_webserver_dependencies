// Copyright 2017-2018 Global Phasing Ltd.
//
// Heuristic methods for working with chains and polymers.
// Includes also a few well-defined functions, such as removal of waters.

#ifndef GEMMI_POLYHEUR_HPP_
#define GEMMI_POLYHEUR_HPP_

#include <vector>
#include "model.hpp"
#include "resinfo.hpp"   // for find_tabulated_residue
#include "calculate.hpp" // for calculate_omega
#include "util.hpp"      // for vector_remove_if

namespace gemmi {

// A simplistic classification. It may change in the future.
// It returns PolymerType which corresponds to _entity_poly.type,
// but here we use only PeptideL, Rna, Dna, DnaRnaHybrid and Unknown.
inline PolymerType check_polymer_type(const ConstResidueSpan& polymer) {
  if (polymer.empty())
    return PolymerType::Unknown;
  size_t counts[ResidueInfo::ELS+1] = {0};
  size_t aa = 0;
  size_t na = 0;
  for (const Residue& r : polymer)
    if (r.entity_type == EntityType::Unknown ||
        r.entity_type == EntityType::Polymer) {
      ResidueInfo info = find_tabulated_residue(r.name);
      if (info.found())
        counts[info.kind]++;
      else if (r.get_ca())
        ++aa;
      else if (r.get_p())
        ++na;
    }
  aa += counts[ResidueInfo::AA] + counts[ResidueInfo::AAD] +
        counts[ResidueInfo::PAA] + counts[ResidueInfo::MAA];
  na += counts[ResidueInfo::RNA] + counts[ResidueInfo::DNA];
  // Exclude water in case this function is called for the whole chain.
  size_t total = polymer.size() - counts[ResidueInfo::HOH];
  if (2 * aa > total)
    return counts[ResidueInfo::AA] >= counts[ResidueInfo::AAD]
           ? PolymerType::PeptideL : PolymerType::PeptideD;
  if (2 * na > total) {
    if (counts[ResidueInfo::DNA] == 0)
      return PolymerType::Rna;
    if (counts[ResidueInfo::RNA] == 0)
      return PolymerType::Dna;
    return PolymerType::DnaRnaHybrid;
  }
  return PolymerType::Unknown;
}

inline PolymerType get_or_check_polymer_type(const Entity* ent,
                                             const ConstResidueSpan& polymer) {
  if (ent && ent->polymer_type != PolymerType::Unknown)
    return ent->polymer_type;
  return check_polymer_type(polymer);
}

inline double calculate_sequence_weight(const std::vector<std::string>& seq,
                                        double unknown=0.) {
  double weight = 0.;
  for (const std::string& item : seq) {
    ResidueInfo res_info = find_tabulated_residue(Entity::first_mon(item));
    weight += res_info.found() ? res_info.weight : unknown;
  }
  return weight - (seq.size() - 1) * h2o_weight();
}

inline std::string one_letter_code(const std::vector<std::string>& seq) {
  std::string r;
  for (const std::string& item : seq)
    r += find_tabulated_residue(Entity::first_mon(item)).fasta_code();
  return r;
}

inline std::string one_letter_code(const ConstResidueSpan& polymer) {
  std::string r;
  for (const Residue& res : polymer.first_conformer())
    r += find_tabulated_residue(res.name).fasta_code();
  return r;
}

inline bool is_polymer_residue(const Residue& res, PolymerType ptype) {
  ResidueInfo info = find_tabulated_residue(res.name);
  // If a standard residue is HETATM we assume that it is in the buffer.
  if (info.found() && info.is_standard() && res.het_flag == 'H')
    return false;
  if (is_polypeptide(ptype))
    return info.found() ? info.is_amino_acid() : !!res.get_ca();
  if (is_polynucleotide(ptype))
    return info.found() ? info.is_nucleic_acid() : !!res.get_p();
  return false;
}

struct AtomNameElement { std::string atom_name; El el; };

inline std::vector<AtomNameElement> get_mainchain_atoms(PolymerType ptype) {
  if (is_polynucleotide(ptype))
    return {{"P", El::P}, {"O5'", El::O}, {"C5'", El::C},
            {"C4'", El::C}, {"O4'", El::O}, {"C3'", El::C}, {"O3'", El::O},
            {"C2'", El::C}, {"O2'", El::O}, {"C1'", El::C}};
  return {{"N", El::N}, {"CA", El::C}, {"C", El::C}, {"O", El::O}};
}

/// distance-based check for peptide bond
inline bool in_peptide_bond_distance(const Atom* a1, const Atom* a2) {
  return a1 && a2 && a1->pos.dist_sq(a2->pos) < sq(1.341 * 1.5);
}
inline bool have_peptide_bond(const Residue& r1, const Residue& r2) {
  return in_peptide_bond_distance(r1.get_c(), r2.get_n());
}

/// distance-based check for phosphodiester bond between nucleotide
inline bool in_nucleotide_bond_distance(const Atom* a1, const Atom* a2) {
  return a1 && a2 && a1->pos.dist_sq(a2->pos) < sq(1.6 * 1.5);
}
inline bool have_nucleotide_bond(const Residue& r1, const Residue& r2) {
  return in_nucleotide_bond_distance(r1.get_o3prim(), r2.get_p());
}

inline bool are_connected(const Residue& r1, const Residue& r2, PolymerType ptype) {
  if (is_polypeptide(ptype))
    return have_peptide_bond(r1, r2);
  if (is_polynucleotide(ptype))
    return have_nucleotide_bond(r1, r2);
  return false;
}

// are_connected2() is less exact, but requires only CA (or P) atoms.
inline bool are_connected2(const Residue& r1, const Residue& r2, PolymerType ptype) {
  auto this_or_first = [](const Atom* a, const Residue& r) -> const Atom* {
    return a || r.atoms.empty() ? a : &r.atoms.front();
  };
  if (is_polypeptide(ptype)) {
    const Atom* a1 = this_or_first(r1.get_ca(), r1);
    const Atom* a2 = this_or_first(r2.get_ca(), r2);
    return a1 && a2 && a1->pos.dist_sq(a2->pos) < sq(5.0);
  }
  if (is_polynucleotide(ptype)) {
    const Atom* a1 = this_or_first(r1.get_p(), r1);
    const Atom* a2 = this_or_first(r2.get_p(), r2);
    return a1 && a2 && a1->pos.dist_sq(a2->pos) < sq(7.5);
  }
  return false;
}

// are_connected3() = are_connected() + fallback to are_connected2()
inline bool are_connected3(const Residue& r1, const Residue& r2, PolymerType ptype) {
  if (is_polypeptide(ptype)) {
    if (const Atom* a1 = r1.get_c())
      if (const Atom* a2 = r2.get_n())
        return a1->pos.dist_sq(a2->pos) < sq(1.341 * 1.5);
    if (const Atom* a1 = r1.get_ca())
      if (const Atom* a2 = r2.get_ca())
        return a1->pos.dist_sq(a2->pos) < sq(5.0);
  } else if (is_polynucleotide(ptype)) {
    if (const Atom* a1 = r1.get_o3prim())
      if (const Atom* a2 = r2.get_p())
        return a1->pos.dist_sq(a2->pos) < sq(1.6 * 1.5);
    if (const Atom* a1 = r1.get_p())
      if (const Atom* a2 = r2.get_p())
        return a1->pos.dist_sq(a2->pos) < sq(7.5);
  }
  return false;
}

inline std::string make_one_letter_sequence(const ConstResidueSpan& polymer) {
  std::string seq;
  const Residue* prev = nullptr;
  PolymerType ptype = check_polymer_type(polymer);
  for (const Residue& residue : polymer.first_conformer()) {
    ResidueInfo info = find_tabulated_residue(residue.name);
    if (prev && !are_connected2(*prev, residue, ptype))
      seq += '-';
    seq += (info.one_letter_code != ' ' ? info.one_letter_code : 'X');
    prev = &residue;
  }
  return seq;
}

inline std::pair<bool,bool> has_entity_types_and_subchains(const Chain& chain) {
  bool has_entity_types = true;
  bool has_subchains = true;
  for (const Residue& res : chain.residues) {
    if (res.subchain.empty())
      has_subchains = false;
    if (res.entity_type == EntityType::Unknown)
      has_entity_types = false;
  }
  return {has_entity_types, has_subchains};
}

inline void add_entity_types(Chain& chain, bool overwrite) {
  PolymerType ptype = check_polymer_type(chain.whole());
  auto it = chain.residues.begin();
  for (; it != chain.residues.end(); ++it)
    if (overwrite || it->entity_type == EntityType::Unknown) {
      if (!is_polymer_residue(*it, ptype))
        break;
      it->entity_type = EntityType::Polymer;
    } else if (it->entity_type != EntityType::Polymer) {
      break;
    }
  for (; it != chain.residues.end(); ++it)
    if (overwrite || it->entity_type == EntityType::Unknown)
      it->entity_type = it->is_water() ? EntityType::Water
                                       : EntityType::NonPolymer;
}

inline void add_entity_types(Structure& st, bool overwrite) {
  for (Model& model : st.models)
    for (Chain& chain : model.chains)
      add_entity_types(chain, overwrite);
}

// The subchain field in the residue is where we store_atom_site.label_asym_id
// from mmCIF files. As of 2018 wwPDB software splits author's chains
// (auth_asym_id) into label_asym_id units:
// * linear polymer,
// * non-polymers (each residue has different separate label_asym_id),
// * and waters.
// Refmac/makecif is doing similar thing but using different naming and
// somewhat different rules (it was written in 1990's before PDBx/mmCIF).
//
// Here we use naming and rules different from both wwPDB and makecif.
// Note: call add_entity_types() first.
inline void assign_subchain_names(Chain& chain, int& nonpolymer_counter) {
  for (Residue& res : chain.residues) {
    res.subchain = chain.name + "-";
    switch (res.entity_type) {
      case EntityType::Polymer:
        res.subchain += 'p';
        break;
      case EntityType::NonPolymer:
        ++nonpolymer_counter;
        // to keep the name short use base36 for 2+ digit numbers:
        // 1, 2, ..., 9, 00, 01, ..., 09, 0A, 0B, ..., 0Z, 10, ...
        if (nonpolymer_counter < 10) {
          res.subchain += ('0' + nonpolymer_counter);
        } else {
          const char base36[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
          int n = nonpolymer_counter - 10;
          if (n < 36)
            res.subchain += '0';
          size_t pos = res.subchain.size();
          while (n != 0) {
            res.subchain.insert(res.subchain.begin() + pos, base36[n % 36]);
            n /= 36;
          }
        }
        break;
      case EntityType::Water:
        res.subchain += 'w';
        break;
      // In the wwPDB branched are kept each in separate auth/label chain.
      // So we have one subchain in chain.
      case EntityType::Branched:
        res.subchain += 'b';
        break;
      case EntityType::Unknown:
        break;
    }
  }
}

inline void assign_subchains(Structure& st, bool force, bool fail_if_unknown=true) {
  for (Model& model : st.models) {
    std::map<std::string, int> counters;
    for (Chain& chain : model.chains) {
      auto has = has_entity_types_and_subchains(chain);
      if (force || !has.second) {
        if (has.first)  // all chain's residues have known entity_type
          assign_subchain_names(chain, counters[chain.name]);
        else if (fail_if_unknown)
          fail("assign_subchains(): missing entity_type in chain " + chain.name);
      }
    }
  }
}

inline void ensure_entities(Structure& st) {
  for (Model& model : st.models)
    for (Chain& chain : model.chains)
      for (ResidueSpan& sub : chain.subchains()) {
        Entity* ent = st.get_entity_of(sub);
        if (!ent) {
          EntityType etype = sub[0].entity_type;
          std::string name;
          if (etype == EntityType::Polymer)
            name = chain.name;
          else if (etype == EntityType::NonPolymer)
            name = sub[0].name + "!";
          else if (etype == EntityType::Water)
            name = "water";
          if (!name.empty()) {
            ent = &impl::find_or_add(st.entities, name);
            ent->entity_type = etype;
            ent->subchains.push_back(sub.subchain_id());
          }
        }
        // ensure we have polymer_type set where needed
        if (ent && ent->entity_type == EntityType::Polymer &&
            ent->polymer_type == PolymerType::Unknown)
          ent->polymer_type = check_polymer_type(sub);
      }
}

inline bool operator==(const Entity::DbRef& a, const Entity::DbRef& b) {
  return a.db_name == b.db_name &&
         a.id_code == b.id_code &&
         a.isoform == b.isoform &&
         a.seq_begin == b.seq_begin && a.seq_end == b.seq_end &&
         a.db_begin == b.db_begin && a.db_end == b.db_end;
}

inline void deduplicate_entities(Structure& st) {
  for (auto i = st.entities.begin(); i != st.entities.end(); ++i)
    if (!i->full_sequence.empty())
      for (auto j = i + 1; j != st.entities.end(); ++j)
        if (j->polymer_type == i->polymer_type &&
            j->full_sequence == i->full_sequence &&
            j->dbrefs == i->dbrefs) {
          vector_move_extend(i->subchains, std::move(j->subchains));
          st.entities.erase(j--);
        }
}

inline void setup_entities(Structure& st) {
  add_entity_types(st, /*overwrite=*/false);
  assign_subchains(st, /*force=*/false);
  ensure_entities(st);
  deduplicate_entities(st);
}


/// Assign Residue::is_cis based on omega angle
template<class T> void assign_cis_flags(T& obj) {
  for (auto& child : obj.children())
    assign_cis_flags(child);
}
inline void assign_cis_flags(Chain& chain) {
  for (Residue& res : chain.residues) {
    bool cis = false;
    if (res.entity_type == EntityType::Polymer)
      if (const Residue* next = chain.next_residue(res))
        if (have_peptide_bond(res, *next))
          if (std::fabs(calculate_omega(res, *next)) < rad(30.))
            cis = true;
    res.is_cis = cis;
  }
}

// Remove waters. It may leave empty chains.
template<class T> void remove_waters(T& obj) {
  for (auto& child : obj.children())
    remove_waters(child);
}
template<> inline void remove_waters(Chain& ch) {
  vector_remove_if(ch.residues,
                   [](const Residue& res) { return res.is_water(); });
}

// Remove ligands and waters. It may leave empty chains.
template<class T> void remove_ligands_and_waters(T& obj) {
  for (auto& child : obj.children())
    remove_ligands_and_waters(child);
}
template<> inline void remove_ligands_and_waters(Chain& ch) {
  PolymerType ptype = check_polymer_type(ch.whole());
  vector_remove_if(ch.residues, [&](const Residue& res) {
      if (res.entity_type == EntityType::Unknown) {
        // TODO: check connectivity
        return !is_polymer_residue(res, ptype);
      }
      return res.entity_type != EntityType::Polymer;
  });
}

// Trim to alanine. Returns true if trimmed, false if it's (likely) not AA.
inline bool trim_to_alanine(Residue& res) {
  static const std::pair<std::string, El> ala_atoms[6] = {
    {"N", El::N}, {"CA", El::C}, {"C", El::C}, {"O", El::O}, {"CB", El::C},
    {"OXT", El::O}
  };
  if (res.get_ca() == nullptr)
    return false;
  vector_remove_if(res.atoms, [](const Atom& a) {
      for (const auto& name_el : ala_atoms)
        if (a.name == name_el.first && a.element == name_el.second)
          return false;
      return true;
  });
  return true;
}

inline void trim_to_alanine(Chain& chain) {
  for (Residue& res : chain.residues)
    trim_to_alanine(res);
}

} // namespace gemmi
#endif
