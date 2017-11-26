#ifndef PE_H
#define PE_H

#include "../../plugins/plugins.h"
#include "pe_headers.h"
#include "pe_imports.h"
#include "pe_utils.h"

#define RVA_POINTER(type, rva) (pointer<type>(rvaToOffset(rva)))

namespace REDasm {

class PeFormat: public FormatPluginT<ImageDosHeader>
{
    private:
        enum PeType { None, VisualBasic, Borland };

    public:
        PeFormat();
        virtual const char* name() const;
        virtual u32 bits() const;
        virtual const char* processor() const;
        virtual offset_t offset(address_t address) const;
        virtual Analyzer* createAnalyzer(DisassemblerFunctions *dfunctions) const;
        virtual bool load(u8 *rawformat);

    private:
        u64 rvaToOffset(u64 rva) const;
        void loadSections();
        void loadExports();
        void loadImports();

    private:
        template<typename THUNK, int ordinalflag> void readDescriptor(const ImageImportDescriptor& importdescriptor);

    private:
        ImageDosHeader* _dosheader;
        ImageNtHeaders* _ntheaders;
        ImageSectionHeader* _sectiontable;
        ImageDataDirectory* _datadirectory;
        u64 _petype, _imagebase, _sectionalignment, _entrypoint;
};

template<typename THUNK, int ordinalflag> void PeFormat::readDescriptor(const ImageImportDescriptor& importdescriptor)
{
    // Check if OFT exists
    THUNK* thunk = RVA_POINTER(THUNK, importdescriptor.OriginalFirstThunk ? importdescriptor.OriginalFirstThunk :
                                                                            importdescriptor.FirstThunk);

    std::string descriptorname = RVA_POINTER(const char, importdescriptor.Name);
    std::transform(descriptorname.begin(), descriptorname.end(), descriptorname.begin(), ::tolower);

    if(descriptorname.find("msvbvm") != std::string::npos)
        this->_petype = PeType::VisualBasic;

    for(size_t i = 0; thunk[i]; i++)
    {
        std::string importname;
        address_t address = this->_imagebase + (importdescriptor.FirstThunk + (i * sizeof(THUNK))); // Instructions refers to FT

        if(!(thunk[i] & ordinalflag))
        {
            ImageImportByName* importbyname = RVA_POINTER(ImageImportByName, thunk[i]);
            importname = PEUtils::importName(descriptorname, reinterpret_cast<const char*>(&importbyname->Name));
        }
        else
        {
            u16 ordinal = (ordinalflag ^ thunk[i]);

            if(!PEImports::importName(descriptorname, ordinal, importname))
                importname = PEUtils::importName(descriptorname, ordinal);
            else
                importname = PEUtils::importName(descriptorname, importname);
        }

        this->defineSymbol(address, importname, SymbolTypes::Import);
    }
}

DECLARE_FORMAT_PLUGIN(PeFormat, pe)

}

#endif // PE_H