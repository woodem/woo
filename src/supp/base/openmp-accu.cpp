
#include"../pyutil/doc_opts.hpp"
#include"../pyutil/except.hpp"
#include"openmp-accu.hpp"

namespace woo{
	/*
	!! THIS WRAPPER IS CURRENTLY UNUSED !! (and might stay unused forever)
	There is a simple to-list converter in src/supp/pyutil/convertes.hpp instead.
	*/
	template<typename Scalar>
	class OpenMPArrayAccuVisitor{
		typedef OpenMPArrayAccumulator<Scalar> AAccuT;
		public:
		static void indexOk(const AAccuT& self, int i){ if(i<0 || i>=self.size()) woo::IndexError("Index {} out of range [0..{}〉",i,self.size()); }

		template<class PyClass>
		static void visit(PyClass& cl){
			cl
			.def("__len__",[](const AAccuT& self){ return self.size();})
			.def("__getitem__",[](const AAccuT& self, int ix){ indexOk(self,ix); return self.get(ix); })
			.def("__setitem__",[](AAccuT& self, int ix, const Scalar& val){ indexOk(self,ix); self.set(ix,val); })
			.def("reset",[](AAccuT& self, int ix){ indexOk(self,ix); self.reset(ix); })
			.def("resetAll",[](AAccuT& self){ self.resetAll(); })
			.def("resize",[](AAccuT& self, int size){ self.resize(size); })
			.def("raw",[](AAccuT& self){ return self.getPerThreadData(); })
			;
		}
	};
	#if 0
		class OpenMPAccuVisitor{
			public:
			template<class PyClass>
			static void visit(PyClass& cl){
				cl
				.def("__len__",[](const T& self){ return self.size());})
				.def("__getitem__",[](const T& self, int ix){ if(i<0 || i>=self.size()) throw std::runtime_error("Index "+to_string(i)+" out of range 0.."+to_string(self.size())); return self.get(i); }
			}
		}
	#endif
	void registerOpenMPAccuClassesInPybind11(py::module& mod){
		WOO_SET_DOCSTRING_OPTS;
		#if 0
			py::class_<OpenMPArrayAccumulator<int>> ompaai(mod,"OpenMPArrayAccumulator<int>");
			OpenMPArrayAccuVisitor<int>::visit(ompaai);
			py::class_<OpenMPArrayAccumulator<Real>> ompaar(mod,"OpenMPArrayAccumulator<Real>");
			OpenMPArrayAccuVisitor<Real>::visit(ompaar);
		#endif
	};
}
