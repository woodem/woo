#pragma once

#include"../base/Types.hpp"
#include"../base/Math.hpp"
#include"../base/CompUtils.hpp"
#include"../pyutil/gil.hpp"

// attribute flags
namespace woo{
	#define ATTR_FLAGS_VALUES noSave=(1<<0), readonly=(1<<1), triggerPostLoad=(1<<2), hidden=(1<<3), noGuiResize=(1<<4), noGui=(1<<5), pyByRef=(1<<6), static_=(1<<7), multiUnit=(1<<8), noDump=(1<<9), activeLabel=(1<<10), rgbColor=(1<<11), filename=(1<<12), existingFilename=(1<<13), dirname=(1<<14), namedEnum=(1<<15), colormap=(1<<16), deprecated=(1<<17)
	// this will disappear later
	namespace Attr { enum flags { ATTR_FLAGS_VALUES }; }

	// prohibit copies, only references should be passed around

	// see http://stackoverflow.com/questions/10570589/template-member-function-specialized-on-pointer-to-data-member and http://ideone.com/95xIt for the logic behind
	// store data here, for uniform access from python
	// derived classes only hide compile-time options
	// and contain named-param accessors, to retain the template type when chained
	struct __attribute__((visibility("hidden"))) AttrTraitBase /*:boost::noncopyable*/{
		// keep in sync with py/wrapper/wooWrapper.cpp !
		enum class Flags { ATTR_FLAGS_VALUES };
		// do not access those directly; public for convenience when accessed from python
		int _flags;
		int _currUnit;
		string _doc, _name, _className, _cxxType,_startGroup, _hideIf;
		vector<string> _unit;
		vector<string> _bits;
		bool _bitsRw; // bits will be python-writable even if corresponding flag attr is not
		vector<pair<string,Real>> _prefUnit;
		vector<vector<pair<string,Real>>> _altUnits;
		map<int,vector<string>> _enumNum2Names; // map value to list of alternative names (the first one is preferred)
		map<string,int> _enumName2Num; // filled automatically from _enumNames

		// avoid throwing exceptions when not initialized, just return None
		void _resetInternalPythonObjects() { _ini=_range=_choice=_buttons=_pyType=[]()->py::object{ return py::none(); }; }
		AttrTraitBase(): _flags(0)              { _resetInternalPythonObjects(); }
		AttrTraitBase(int flags): _flags(flags) { _resetInternalPythonObjects(); }

		// manually destruct members containing python objects with GIL to avoid crash at shutdown
		// e.g. when _ini was holding a make_shared<Plot>(), it would cause crash at shutdown
		// due to shared_ptr not holding GIL when releasing the object possibly created in python
		~AttrTraitBase(){ 
			if(Py_IsInitialized()){ GilLock gil; _resetInternalPythonObjects(); }
			_resetInternalPythonObjects();
		}

		std::function<py::object()> _ini;
		std::function<py::object()> _range;
		std::function<py::object()> _choice;
		std::function<py::object()> _buttons;
		std::function<py::object()> _pyType; // only used by the UI
		// getters (setters below, to return the same reference type)
		#define ATTR_FLAG_DO(flag,isFlag) bool isFlag() const { return _flags&(int)Flags::flag; }
			ATTR_FLAG_DO(noSave,isNoSave)
			ATTR_FLAG_DO(readonly,isReadonly)
			ATTR_FLAG_DO(triggerPostLoad,isTriggerPostLoad)
			ATTR_FLAG_DO(hidden,isHidden)
			ATTR_FLAG_DO(noGuiResize,isNoGuiResize)
			ATTR_FLAG_DO(noGui,isNoGui)
			ATTR_FLAG_DO(pyByRef,isPyByRef)
			ATTR_FLAG_DO(static_,isStatic)
			ATTR_FLAG_DO(multiUnit,isMultiUnit)
			ATTR_FLAG_DO(noDump,isNoDump)
			ATTR_FLAG_DO(activeLabel,isActiveLabel)
			ATTR_FLAG_DO(rgbColor,isRgbColor)
			ATTR_FLAG_DO(filename,isFilename)
			ATTR_FLAG_DO(existingFilename,isExistingFilename)
			ATTR_FLAG_DO(dirname,isDirname)
			ATTR_FLAG_DO(namedEnum,isNamedEnum)
			ATTR_FLAG_DO(colormap,isColormap)
			ATTR_FLAG_DO(deprecated,isDeprecated)
		#undef ATTR_FLAG_DO
		py::object pyGetIni()const{ return _ini(); }
		py::object pyGetRange()const{ return _range(); }
		py::object pyGetChoice()const{ return _choice(); }
		py::object pyGetButtons(){ return _buttons(); }
		// _pyType is settable from python
		py::object pyGetPyType(){ return _pyType(); }
		void pySetPyType(py::object value){ _pyType=std::function<py::object()>([value](){ return value; }); }
		py::object pyGetBits()const{ return py::cast(_bits); }
		py::object pyUnit() const { return py::cast(_unit); }
		// define alternative units: name and factor, by which the base unit is multiplied to obtain this one
		py::object pyAltUnits() const { return py::cast(_altUnits); }
		py::object pyPrefUnit(){
			// return base unit if preferred not specified
			for(size_t i=0; i<_prefUnit.size(); i++) if(_prefUnit[i].first.empty()) _prefUnit[i]=pair<string,Real>(_unit[i],1.);
			return py::cast(_prefUnit);
		}

		string pyStr(){ return "<AttrTrait '"+_name+"', flags="+to_string(_flags)+" @ '"+ptr_to_string(this)+">"; }


		void namedEnum_validValues(std::ostream& os, const string& pre0="", const string& post0="", const string& pre="", const string& post="") const {
			bool first=true;
			for(const auto& iss: _enumNum2Names){
				os<<(first?"":", "); first=false;
				const auto& ss(iss.second);
				assert(!ss.empty());
				os<<pre0<<ss[0]<<post0<<" (";
				for(size_t i=1; i<ss.size(); i++){
					os<<(i==1?"":", ")<<pre<<ss[i]<<post;
				}
				os<<(ss.size()>1?"; ":"")<<iss.first<<")";
			}
		}
		std::string namedEnum_pyValidValues(const string& pre0, const string& post0, const string& pre, const string& post) const {
			std::ostringstream oss;
			namedEnum_validValues(oss,pre0,post0,pre,post);
			return oss.str();
		}
		int namedEnum_name2num(py::object o) const {
			py::extract<int> i(o);
			if(i.check()){
				if(_enumNum2Names.count(i())==0){
					std::ostringstream oss;
					oss<<i<<" invalid for "<<_className+"."+_name<<". Valid values are: ";
					namedEnum_validValues(oss);
					oss<<".";
					woo::ValueError(oss.str());
				} else return i();
			}
			// this also extracts utf-8 encoded unicode objects, due to custom conerter in the global registry :)
			py::extract<string> s(o);
			if(s.check()){
				string su(s());
				auto I=_enumName2Num.find(su);
				if(I==_enumName2Num.end()){
					std::ostringstream oss;
					oss<<"'"<<su<<"' invalid for "<<_className<<"."<<_name<<". Valid values are: ";
					namedEnum_validValues(oss);
					oss<<".";
					woo::ValueError(oss.str());
				}
				return I->second;
			}
			woo::TypeError("Named enumeration "+_className+"."+_name+" can only be assigned string or int (not .");
			// make compiler happy
			throw std::logic_error("Unreachable code in AttrTrait::namedEnum_name2num."); 
		}
		string namedEnum_num2name(int i) const {
			auto I=_enumNum2Names.find(i);
			if(I==_enumNum2Names.end()) throw std::logic_error("Internal (c++) value of "+_className+"."+_name+" is "+to_string(i)+", which is not valid according to AttrTrait.");
			return I->second[0];
		}

		static void pyRegisterClass(py::module_ mod){
			py::class_<AttrTraitBase>(mod,"AttrTrait")
				.def_property_readonly("noSave",&AttrTraitBase::isNoSave)
				.def_property_readonly("readonly",&AttrTraitBase::isReadonly)
				.def_property_readonly("triggerPostLoad",&AttrTraitBase::isTriggerPostLoad)
				.def_property_readonly("hidden",&AttrTraitBase::isHidden)
				.def_property_readonly("noGuiResize",&AttrTraitBase::isNoGuiResize)
				.def_property_readonly("noGui",&AttrTraitBase::isNoGui)
				.def_property_readonly("pyByRef",&AttrTraitBase::isPyByRef)
				.def_property_readonly("static",&AttrTraitBase::isStatic)
				.def_property_readonly("multiUnit",&AttrTraitBase::isMultiUnit)
				.def_property_readonly("noDump",&AttrTraitBase::isNoDump)
				.def_property_readonly("activeLabel",&AttrTraitBase::isActiveLabel)
				.def_property_readonly("rgbColor",&AttrTraitBase::isRgbColor)
				.def_property_readonly("filename",&AttrTraitBase::isFilename)
				.def_property_readonly("existingFilename",&AttrTraitBase::isExistingFilename)
				.def_property_readonly("dirname",&AttrTraitBase::isDirname)
				.def_property_readonly("namedEnum",&AttrTraitBase::isNamedEnum)
				.def_property_readonly("colormap",&AttrTraitBase::isColormap)
				.def_property_readonly("deprecated",&AttrTraitBase::isDeprecated)
				.def("namedEnum_validValues",&AttrTraitBase::namedEnum_pyValidValues,WOO_PY_ARGS(py::arg("pre0")="",py::arg("post0")="",py::arg("pre")="",py::arg("post")=""),"Valid values for named enum. *pre* and *post* are prefixed/suffixed to each possible value (used for formatting), *pre0* and *post0* are used with the first (primary/preferred) value.")
				.def_readonly("_flags",&AttrTraitBase::_flags)
				// non-flag attributes
				.def_readonly("doc",&AttrTraitBase::_doc)
				.def_readonly("cxxType",&AttrTraitBase::_cxxType)
				.def_readonly("name",&AttrTraitBase::_name)
				.def_readonly("className",&AttrTraitBase::_className)
				.def_property_readonly("unit",&AttrTraitBase::pyUnit)
				.def_property_readonly("prefUnit",&AttrTraitBase::pyPrefUnit)
				.def_property_readonly("altUnits",&AttrTraitBase::pyAltUnits)
				.def_readonly("startGroup",&AttrTraitBase::_startGroup)
				.def_readonly("hideIf",&AttrTraitBase::_hideIf)
				.def_property_readonly("ini",&AttrTraitBase::pyGetIni)
				.def_property_readonly("range",&AttrTraitBase::pyGetRange)
				.def_property_readonly("choice",&AttrTraitBase::pyGetChoice)
				.def_property_readonly("bits",&AttrTraitBase::pyGetBits)
				.def_property_readonly("buttons",&AttrTraitBase::pyGetButtons)
				.def_property("pyType",&AttrTraitBase::pyGetPyType,&AttrTraitBase::pySetPyType)
				//.def_readwrite("pyType",&AttrTraitBase::pyPyType)
				.def("__str__",&AttrTraitBase::pyStr)
				.def("__repr__",&AttrTraitBase::pyStr)
				.def("_resetInternalPythonObjects",&AttrTraitBase::_resetInternalPythonObjects,"Internal purposes only: release any internally-help python objects of this trait. The trait will be invalid at this point. Should be called at Python shutdown.")
			;
		}
	};
	template<int _compileFlags=0>
	struct __attribute__((visibility("hidden"))) AttrTrait: public AttrTraitBase {
		enum { compileFlags=_compileFlags };
		AttrTrait(): AttrTraitBase(_compileFlags) { };
		AttrTrait(int f){ _flags=f; } // for compatibility
		// setters
		/*
			Some flags may only be set as template argument (since they select templated code),
			not after trait construction. Their setters are not provided below. Those are:

			* noSave
			* triggerPostLoad
			* hidden
			* namedEnum

		*/
		#define ATTR_FLAG_DO(flag,isFlag) AttrTrait& flag(bool val=true){ if(val) _flags|=(int)Flags::flag; else _flags&=~((int)Flags::flag); return *this; } bool isFlag() const { return _flags&(int)Flags::flag; }
			// dynamically modifiable without harm
			ATTR_FLAG_DO(pyByRef,isPyByRef)
			ATTR_FLAG_DO(readonly,isReadonly)
			ATTR_FLAG_DO(noGuiResize,isNoGuiResize)
			ATTR_FLAG_DO(noGui,isNoGui)
			ATTR_FLAG_DO(static_,isStatic)
			ATTR_FLAG_DO(multiUnit,isMultiUnit)
			ATTR_FLAG_DO(noDump,isNoDump)
			ATTR_FLAG_DO(activeLabel,isActiveLabel)
			ATTR_FLAG_DO(rgbColor,isRgbColor)
			ATTR_FLAG_DO(filename,isFilename)
			ATTR_FLAG_DO(existingFilename,isExistingFilename)
			ATTR_FLAG_DO(dirname,isDirname)
			ATTR_FLAG_DO(colormap,isColormap) // requires namedEnum (error at runtime if not)
            //ATTR_FLAG_DO(deprecated,isDeprecated)
		#undef ATTR_FLAG_DO

        // deprecated sets noDump as side-effect, hence is defined by hand
        AttrTrait& deprecated(){ _flags|=(int)Flags::deprecated; _flags|=(int)Flags::noDump; _flags|=(int)Flags::noGui; return *this; }
        bool isDeprecated(){ return _flags&(int)Flags::deprecated; }

		AttrTrait& name(const string& s){ _name=s; return *this; }
		AttrTrait& className(const string& s){ _className=s; return *this; }
		AttrTrait& doc(const string& s){ _doc=s; return *this; }
		AttrTrait& cxxType(const string& s){ _cxxType=s; return *this; }
		AttrTrait& unit(const string& s){
			if(!_unit.empty() && !isMultiUnit()){
				cerr<<"ERROR: AttrTrait must be declared .multiUnit() before additional units are specified."<<endl;
				abort();
			}
			_unit.push_back(s); _altUnits.resize(_unit.size()); _prefUnit.resize(_unit.size());
			return *this;
		}
		AttrTrait& prefUnit(const string& s){
			if(_unit.empty() && !isMultiUnit()){
				cerr<<"ERROR: Set AttrTrait.unit() before AttrTrait.prefUnit()."<<endl;;
				abort();
			}
			Real mult;
			if(s==_unit.back()) mult=1.;
			else{
				/*find multiplier*/
				auto I=std::find_if(_altUnits.back().begin(),_altUnits.back().end(),[&](const pair<string,Real>& a)->bool{ return a.first==s; });
				if(I!=_altUnits.back().end()){ mult=I->second; }
				else throw std::runtime_error("AttrTrait for "+_name+": prefUnit(\""+s+"\") does not match any .altUnits()");
			}
			_prefUnit[_unit.size()-1]={s,mult};
			return *this;
		}
		AttrTrait& altUnits(const vector<pair<string,Real>>& m, bool clearPrevious=false){
			if(_unit.empty() && !isMultiUnit()){
				cerr<<"ERROR: Set AttrTrait.unit() before AttrTrait.altUnits()."<<endl;
				abort();
			}
			auto& alt(_altUnits[_unit.size()-1]);
			if(clearPrevious) alt=m;
			else alt.insert(alt.end(),m.begin(),m.end());
			return *this;
		}
		AttrTrait& startGroup(const string& s){ _startGroup=s; return *this; }
		AttrTrait& hideIf(const string& h){ _hideIf=h; return *this; }

		// https://stackoverflow.com/a/17201138/761090
		template<typename T,typename std::enable_if<std::is_base_of<py::handle,T>::value>::type* =nullptr> AttrTrait& ini(const T t){ _ini=std::function<py::object()>([=]()->py::object{ return t; }); return *this; }
		template<typename T,typename std::enable_if<!std::is_base_of<py::handle,T>::value>::type* =nullptr> AttrTrait& ini(const T t){ _ini=std::function<py::object()>([=]()->py::object{ return py::cast(t); }); return *this; }
		AttrTrait& ini(){ _ini=std::function<py::object()>([]()->py::object{ return py::object(); }); return *this; }

		template<typename Scalar>
		AttrTrait& range(const Eigen::Matrix<Scalar,2,1>& t){ _range=std::function<py::object()>([=]()->py::object{ return py::cast(t);} ); return *this; }
		// AttrTrait& range(const Vector2r& t){ _range=std::function<py::object()>([=]()->py::object{ return py::cast(t);} ); return *this; }

		// choice from integer values
		AttrTrait& choice(const vector<int>& t){ _choice=std::function<py::object()>([=]()->py::object{ return py::cast(t);} ); return *this; }
		// choice from integer values, represented by descriptions
		AttrTrait& choice(const vector<pair<int,string>>& t){ _choice=std::function<py::object()>([=]()->py::object{ return py::cast(t);} ); return *this; }
		AttrTrait& choice(const vector<string>& t){ _choice=std::function<py::object()>([=]()->py::object{ return py::cast(t);} ); return *this; }
		// bitmask where each bit is represented by a string (given from the left)
		AttrTrait& bits(const vector<string>& t, bool rw=false){ _bits=t; _bitsRw=rw; return *this; }

		AttrTrait& namedEnum(const map<int,vector<string>>& _n){
			static_assert(compileFlags&(int)Flags::namedEnum,"Attr::namedEnum flag must be used as template parameter with .namedEnum(): AttrTrait<Attr::namedEnum|...>().namedEnum(...).");
			_enumNum2Names=_n;
			vector<string> ch;
			for(const auto& iss: _enumNum2Names){
				if(iss.second.empty()) woo::ValueError("AttrTrait.namedEnum: no names provided for "+to_string(iss.first));
				for(const string& n: iss.second) _enumName2Num[n]=iss.first;
				ch.push_back(iss.second[0]); // will be shown as choice in the UI
			}
			choice(ch);
			return *this;
		}
		// pre-filled choice with colormap
		AttrTrait& colormapChoice(bool includeDefault=true){
			static_assert(compileFlags&(int)Flags::namedEnum,"Attr::namedEnum flag must be used as template parameter with .colormapChoice(): AttrTrait<Attr::namedEnum|...>().colormapChoice(...).");
			colormap(true); // set the bit, so that python can put sampler next to the name in the UI
			map<int,vector<string>> pairs; if(includeDefault) pairs.insert({-1,{"default",""}}); for(int i=0; i<(int)CompUtils::colormaps.size(); i++) pairs.insert({i,{CompUtils::colormaps[i].name}}); return namedEnum(pairs);
		}

		AttrTrait& buttons(const vector<string>& b, bool showBefore=true){ _buttons=std::function<py::object()>([=]()->py::object{ return py::make_tuple(py::cast(b),showBefore);}); return *this; }

		// shorthands for common units
		// each new unit MUST be added as an attribute to core/Test.hpp
		// so that it is know to python!!
		AttrTrait& angleUnit(){ unit("rad"); altUnits({{"deg",180/M_PI}}); return *this; }
		AttrTrait& timeUnit(){ unit("s"); altUnits({{"ms",1e3},{"μs",1e6},{"day",1./(60*60*24)},{"year",1./(60*60*24*365)}}); return *this; }
		AttrTrait& lenUnit(){ unit("m"); altUnits({{"mm",1e3},{"cm",1e2}}); return *this; }
		AttrTrait& areaUnit(){ unit("m²"); altUnits({{"mm²",1e6}}); return *this; }
		AttrTrait& volUnit(){ unit("m³"); altUnits({{"mm³",1e9}}); return *this; }
		AttrTrait& velUnit(){ unit("m/s"); altUnits({{"km/h",1e-3*3600},{"m/min",60}}); return *this; }
		AttrTrait& accelUnit(){ unit("m/s²"); return *this; }
		AttrTrait& massUnit(){ unit("kg"); altUnits({{"g",1e3},{"t",1e-3}}); return *this; }
		AttrTrait& angVelUnit(){ unit("rad/s"); altUnits({{"rot/s",1/(2*M_PI)},{"rot/min",60.*(1./(2*M_PI))},{"rpm",60.*(1./(2*M_PI))},{"rph",3600.*(1./(2*M_PI))}}); return *this; }
		AttrTrait& angMomUnit(){ unit("N·m·s"); return *this; }
		AttrTrait& inertiaUnit(){ unit("kg·m²"); return *this; }
		AttrTrait& forceUnit(){ unit("N"); altUnits({{"kN",1e-3},{"MN",1e-6}}); return *this; }
		AttrTrait& torqueUnit(){ unit("N·m"); altUnits({{"kN·m",1e-3},{"MN·m",1e-6}}); return *this; }
		AttrTrait& pressureUnit(){ unit("Pa"); altUnits({{"kPa",1e-3},{"MPa",1e-6},{"GPa",1e-9}}); return *this; }
		AttrTrait& stiffnessUnit(){ return pressureUnit(); }
		AttrTrait& massRateUnit(){ unit("kg/s"); altUnits({{"t/h",1e-3*3600},{"t/y",1e-3*(24*3600*365)},{"Mt/y",1e-6*1e-3*(24*3600*365)}}); return *this; }
		AttrTrait& densityUnit(){ unit("kg/m³"); altUnits({{"t/m³",1e-3},{"g/cm³",1e-3}}); return *this; }
		AttrTrait& fractionUnit(){ unit("-"); altUnits({{"%",1e2},{"‰",1e3},{"ppm",1e6}}); return *this; }
		AttrTrait& surfEnergyUnit(){ unit("J/m²"); return *this; }

	};
	#undef ATTR_FLAGS_VALUES

	// overloaded for seamless expansion of macros
	// empty value returns default traits
	inline AttrTrait<0> makeAttrTrait(){ return AttrTrait<0>(); }
	// passing an AttrTrait object returns itself
	template<int flags>
	AttrTrait<flags> makeAttrTrait(const AttrTrait<flags>& at){ return at; }

	struct ClassTrait{
		string pyStr(){ return "<ClassTrait '"+_name+"' @ "+ptr_to_string(this)+">"; }
		string _doc;
		string _name;
		string title;
		string intro;
		vector<string> docOther;
		string _file;
		long _line;

		string getDoc() const { return _doc; }
		ClassTrait& file(const string& __file){ _file=__file; return *this; }
		ClassTrait& line(const long __line){ _line=__line; return *this; }
		ClassTrait& doc(const string __doc){ _doc=__doc; return *this; }
		ClassTrait& name(const string& __name){ _name=__name; return *this; }
		ClassTrait& section(const string& _title, const string& _intro, const vector<string>& _docOther){ title=_title; intro=_intro; docOther=_docOther; return *this; }
		static void pyRegisterClass(py::module_ mod){
			py::class_<ClassTrait,shared_ptr<ClassTrait>>(mod,"ClassTrait")
				.def_readonly("title",&ClassTrait::title)
				.def_readonly("intro",&ClassTrait::intro)
				.def_readonly("file",&ClassTrait::_file)
				.def_readonly("line",&ClassTrait::_line)
				.add_property_readonly("docOther",[](const shared_ptr<ClassTrait>& c){return c->docOther; })
				.def_readonly("doc",&ClassTrait::_doc)
				.def_readonly("name",&ClassTrait::_name)
				.def("__str__",&ClassTrait::pyStr)
				.def("__repr__",&ClassTrait::pyStr)
			;
		}
	};
	inline ClassTrait makeClassTrait(){ return ClassTrait(); }
	inline ClassTrait makeClassTrait(const string& _doc){ return ClassTrait().doc(_doc); }
	inline ClassTrait makeClassTrait(ClassTrait& c){ return c; }
};

