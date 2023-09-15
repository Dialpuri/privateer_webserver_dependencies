/*!  \file clipper_gemmi.h
  Header file for GEMMI wrapper
*/


#ifndef CLIPPER_GEMMI
#define CLIPPER_GEMMI


#include <gemmi/model.hpp>
#include <clipper/clipper.h>


namespace clipper{

    namespace gemmi {
      typedef ::gemmi::Structure CGStructure;
      typedef ::gemmi::Metadata CGMetadata;
      typedef ::gemmi::CoorFormat CGCoorFormat;
      typedef ::gemmi::SpaceGroup CGSG;
      typedef ::gemmi::NcsOp CGNcsOp;
      typedef ::gemmi::Entity CGEntity;
      typedef ::gemmi::Connection CGConnection;
      typedef ::gemmi::Helix CGHelix;
      typedef ::gemmi::Sheet CGSheet;
      typedef ::gemmi::Assembly CGAssembly;
      typedef ::gemmi::Transform CGTransform;

      typedef ::gemmi::Model CGModel; 
      typedef ::gemmi::Chain CGChain; 
      typedef ::gemmi::Residue CGResidue;
      typedef ::gemmi::Atom CGAtom;
      typedef ::gemmi::CRA CGCRA;
      typedef ::gemmi::ResidueId CGRId;
      
      typedef ::gemmi::Model * PGModel;
      typedef ::gemmi::Chain * PGChain;
      typedef ::gemmi::Residue * PGResidue;
      typedef ::gemmi::Atom * PGAtom;
    }

    //! Options for writing to file. Adapted from Gemmi
    struct FileOptions {
      bool ExpandNCS = false;
      bool RemoveH = false;
      bool RemoveH2O = false;
      bool RemoveLigH2O = false;
      bool TrimAla = false;
      bool SegmentAsChain = false;
      bool PDBX_Format = false;
      String Blockname;
      bool ShortTER = true;
      bool LinkR = false;
      bool MinimalPdb = true;
    };

    class GemmiStructure
    {
      public:
        //! get IDs for model, chain, residue or atom
        String GetID_str( const String& model_name, const gemmi::CGCRA& cra, const String& hnd );
        //! get spacegroup and return in clipper::Spacegroup format
        Spacegroup spacegroup() const;
        //! get cell and return in clipper::Cell format
        Cell cell() const;
        //! set spacegroup to Gemmi Structure
        void set_spacegroup( const Spacegroup& spacegroup );
        //! set cell to Gemmi Structure
        void set_cell( const Cell& cell );
	//! print experimental method
	String get_exptlmethod() const;

      // protected:      
        gemmi::CGStructure structure_;
    };

} // namespace clipper


#endif
