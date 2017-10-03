# encoding: utf-8
'''Define IO routines for arbitrary objects.'''
from __future__ import print_function
import future.utils
from builtins import str
from past.builtins import unicode
import past.builtins
py3k=future.utils.PY3
# various monkey-patches for wrapped c++ classes
import woo.core
import woo.system
import woo.document
#import woo.dem
from minieigen import * # for recognizing the types

if py3k: from io import StringIO
else: import StringIO # cStringIO does not handle unicode, so stick with the slower one

from woo.core import Object
import woo._customConverters # to make sure they are loaded already

import codecs
import pickle
import json
import minieigen
import numpy
import sys
# we don't support h5py on Windows, as it is too complicated to install
# this is an ugly hack :|
try:
    import h5py
    haveH5py=True
except ImportError:
    haveH5py=False

nan,inf=float('nan'),float('inf') # for values in expressions

# ensure that the string is unicode
def _ensureUnicode(s): return s if isinstance(s,str) else s.decode('utf-8')


def Object_getAllTraits(obj):
    'Return list of all trait objects for this instance, recursively including all parent classes.'
    ret=[]; k=obj.__class__
    while k!=woo.core.Object:
        ret=k._attrTraits+ret
        k=k.__bases__[0]
    return ret

def Object_getAllTraitsWithClasses(obj):
    'Return list of (trait,klass) relevant for *obj*, where *klass* is class (type object) to which each trait belongs.'
    ret=[]; k=obj.__class__
    while k!=woo.core.Object:
        ret=[(trait,k) for trait in k._attrTraits]+ret
        k=k.__bases__[0]
    return ret


# this is not necessary in Python >= 3.0, and older versions of minieigen don't expose that function 
if sys.version_info<(3,0) and hasattr(minieigen,'float2str'):
    def float2str(f): return minieigen.float2str(f)
else:
    def float2str(f): return '%g'%f


htmlHead='<head><meta http-equiv="content-type" content="text/html;charset=UTF-8" /></head><body>\n'

def Object_dumps(obj,format,fragment=False,width=80,noMagic=False,stream=True,showDoc=False,hideWooExtra=False):
    if format not in ('html','expr','json','pickle','genshi'): raise IOError("Unsupported string dump format %s"%format)
    if format=='pickle':
        return pickle.dumps(obj)
    elif format=='json':
        return WooJSONEncoder().encode(obj)
    elif format=='expr':
        return SerializerToExpr(maxWd=width,noMagic=noMagic)(obj)
    elif format=='html':
        return ('' if fragment else htmlHead)+str(SerializerToHtmlTable(showDoc=showDoc,hideWooExtra=hideWooExtra)(obj))+('' if fragment else '</body>')
    elif format=='genshi':
        return SerializerToHtmlTable()(obj,dontRender=True,showDoc=showDoc,hideWooExtra=hideWooExtra)

def Object_dump(obj,out,format='auto',fallbackFormat=None,overwrite=True,fragment=False,width=80,noMagic=False,showDoc=False,hideWooExtra=False):
    '''Dump an object in specified *format*; *out* can be a str/unicode (filename) or a *file* object. Supported formats are: `auto` (auto-detected from *out* extension; raises exception when *out* is an object), `html`, `expr`.'''
    hasFilename=isinstance(out,(str,past.builtins.str))
    if hasFilename:
        import os.path
        if os.path.exists(out) and not overwrite: raise IOError("File '%s' exists (use overwrite=True)"%out)
    if format=='auto':
        if not hasFilename: raise IOError("format='auto' is only possible when a fileName is given.")
        if out.endswith('.html'): format='html'
        elif sum([out.endswith(ext) for ext in ('.expr','expr.gz','expr.bz2')]): format='expr'
        elif sum([out.endswith(ext) for ext in ('.pickle','pickle.gz','pickle.bz2')]): format='pickle'
        elif sum([out.endswith(ext) for ext in ('.json','json.gz','json.bz2')]): format='json'
        elif sum([out.endswith(ext) for ext in ('.xml','.xml.gz','.xml.bz2','.bin','.gz','.bz2')]): format='boost::serialization'
        elif fallbackFormat is not None: format=fallbackFormat
        else: IOError("Output format not deduced for filename '%s' (and fallbackFormat not specified)"%out)
    if format not in ('auto','html','json','expr','pickle','boost::serialization'): raise IOError("Unsupported dump format %s"%format)
    #if not hasFilename and not hasattr(out,'write'): raise IOError('*out* must be filename or file-like object')
    if format in ('boost::serialization',) and not hasFilename: raise IOError("format='boost::serialization' needs filename.")
    # this will go away later
    if format=='boost::serialization':
        if format in ('boost::serialization',) and not isinstance(obj,woo.core.Object): raise IOError("Only instances of woo.core.Object can be saved via boost::serialization")
        if not hasFilename: raise NotImplementedError('Only serialization to files (not to strings) is supported with boost::serialization.')
        if obj.deepcopy.__module__!='woo._monkey.io': raise IOError("boost::serialization formats can only reliably save pure-c++ objects. Given object %s.%s seems to be derived from python. Save using some dump formats."%(obj.__class__.__module__,obj.__class__.__name__))
        obj.save(str(out)) # must convert unicode to str here, so that it can be converted to std::string
    elif format=='auto':
        raise IOError("format='auto' could not guess format from extension (and fallback not given) -- say format='...' or pass an extension which is understood.")
    elif format=='pickle':
        if hasFilename: pickle.dump(obj,open(out,'wb'))
        else: out.write(pickle.dumps(obj))
    elif format in ('expr','html','json'):
        if hasFilename:
            out=codecs.open(out,'wb','utf-8')
        if format=='expr':
            out.write(SerializerToExpr(maxWd=width,noMagic=noMagic)(obj))
        elif format=='json':
            out.write(WooJSONEncoder().encode(obj))
        elif format=='html':
            if not fragment: out.write(htmlHead)
            out.write(str(SerializerToHtmlTable(showDoc=showDoc,hideWooExtra=hideWooExtra)(obj)))
            if not fragment: out.write('</body>')
    else: assert False,'Unreachable.'
        


class SerializerToHtmlTableGenshi(object):
    'Dump given object to HTML table, using the `Genshi <http://genshi.edgewall.org>`_ templating engine; the produced serialization is XHTML-compliant. Do not use this class directly, say ``object.dump(format="html")`` instead.'
    padding=dict(cellpadding='2px')
    splitStrSeq=1
    splitIntSeq=5
    splitFloatSeq=5
    def __init__(self,showDoc=False,maxDepth=8,hideNoGui=False,hideWooExtra=False):
        self.maxDepth=maxDepth
        self.hideNoGui=hideNoGui
        self.hideWooExtra=hideWooExtra
        self.showDoc=showDoc
    def htmlSeq(self,s,insideTable):
        from genshi.builder import tag
        table=tag.table(frame='box',rules='all',width='100%',**self.padding)
        if hasattr(s[0],'__len__') and not isinstance(s[0],(str,unicode,bytes)): # 2d array
            # disregard insideTable in this case
            for r in range(len(s) if type(s)!=AlignedBox3 else 2): # len(s) is sufficient, but some version of minieigen report erroneously that AlignedBox3 has length of 3
                tr=tag.tr()
                for c in range(len(s[0])):
                    tr.append(tag.td(float2str(s[r][c]) if isinstance(s[r][c],float) else str(s[r][c]),align='right',width='%g%%'%(100./len(s[0])-1.)))
                table.append(tr)
            return table
        splitLen=0
        if len(s)>1:
            if isinstance(s[0],int): splitLen=self.splitIntSeq
            elif isinstance(s[0],(str,unicode)): splitLen=self.splitStrSeq
            elif isinstance(s[0],float): splitLen=self.splitFloatSeq
        # 1d array
        if splitLen==0 or len(s)<splitLen:
            ret=table if not insideTable else []
            for e in s: ret.append(tag.td(float2str(e) if isinstance(e,float) else str(e),align='right',width='%g%%'%(100./len(s)-1.)))
        # 1d array, but with lines broken
        else:
            ret=table
            for i,e in enumerate(s):
                if i%splitLen==0:
                    tr=tag.tr()
                tr.append(tag.td(float2str(e) if isinstance(e,float) else str(e),align='right',width='%g%%'%(100./splitLen-1.)))
                # last in the line, or last overall
                if i%splitLen==(splitLen-1) or i==len(s)-1: table.append(tr)
        return ret
    def __call__(self,obj,depth=0,dontRender=False):
        from genshi.builder import tag
        if depth>self.maxDepth: raise RuntimeError("Maximum nesting depth %d exceeded"%self.maxDepth)
        kw=self.padding.copy()
        if depth>0: kw.update(width='100%')
        # was [1:] to omit leading woo./wooExtra., but that is not desirable
        objInExtra=obj.__class__.__module__.startswith('wooExtra.')
        if self.hideWooExtra and objInExtra: head=tag.span(tag.b(obj.__class__.__name__),title=_ensureUnicode(obj.__class__.__doc__))
        else: 
            head=tag.b('.'.join(obj.__class__.__module__.split('.')[0:])+'.'+obj.__class__.__name__)
            head=tag.a(head,href=woo.document.makeObjectUrl(obj),title=_ensureUnicode(obj.__class__.__doc__))
        ret=tag.table(tag.th(head,colspan=3,align='left'),frame='box',rules='all',**kw)
        # get all attribute traits first
        traits=obj._getAllTraits()
        for trait in traits:
            if trait.hidden or (self.hideNoGui and trait.noGui) or trait.noDump or (trait.hideIf and eval(trait.hideIf,globals(),{'self':obj})): continue
            # start new group (additional line)
            if trait.startGroup:
                ret.append(tag.tr(tag.td(tag.i(u'▸ %s'%_ensureUnicode(trait.startGroup)),colspan=3)))
            attr=getattr(obj,trait.name)
            if self.showDoc: tr=tag.tr(tag.td(_ensureUnicode(trait.doc)))
            else:
                try:
                    if self.hideWooExtra and objInExtra: label=tag.span(tag.b(trait.name),title=_ensureUnicode(trait.doc))
                    else: label=tag.a(trait.name,href=woo.document.makeObjectUrl(obj,trait.name),title=_ensureUnicode(trait.doc))
                    tr=tag.tr(tag.td(label))
                except UnicodeEncodeError:
                    print('ERROR: UnicodeEncodeError while formatting the attribute ',obj.__class__.__name__+'.'+trait.name)
                    print('ERROR: the docstring is',trait.doc)
                    raise
            # tr=tag.tr(tag.td(trait.name if not self.showDoc else trait.doc.decode('utf-8')))
            # nested object
            if isinstance(attr,woo.core.Object):
                tr.append([tag.td(self(attr,depth+1),align='justify'),tag.td()])
            # sequence of objects (no units here)
            elif hasattr(attr,'__len__') and len(attr)>0 and isinstance(attr[0],woo.core.Object):
                tr.append(tag.td(tag.ol([tag.li(self(o,depth+1)) for o in attr])))
            else:
                # !! make deepcopy so that the original object is not modified !!
                import copy
                attr=copy.deepcopy(attr)
                if not trait.multiUnit: # the easier case
                    if not trait.prefUnit: unit=u'−'
                    else:
                        unit=_ensureUnicode(trait.prefUnit[0][0])
                        # create new list, where entries are multiplied by the multiplier
                        if type(attr)==list: attr=[a*trait.prefUnit[0][1] for a in attr]
                        else: attr=attr*trait.prefUnit[0][1]
                else: # multiple units
                    unit=[]
                    wasList=isinstance(attr,list)
                    if not wasList: attr=[attr] # handle uniformly
                    for i in range(len(attr)):
                        attr[i]=[attr[i][j]*trait.prefUnit[j][1] for j in range(len(attr[i]))]
                    for pu in trait.prefUnit:
                        unit.append(_ensureUnicode(pu[0]))
                    if not wasList: attr=attr[0]
                    unit=', '.join(unit)
                # sequence type, or something similar                
                if hasattr(attr,'__len__') and not isinstance(attr,(str,unicode,bytes)):
                    if len(attr)>0:
                        tr.append(tag.td(self.htmlSeq(attr,insideTable=False),align='right'))
                    else:
                        tr.append(tag.td(tag.i('[empty]'),align='right'))
                else:
                    tr.append(tag.td(float2str(attr) if isinstance(attr,float) else str(attr),align='right'))
                if unit:
                    tr.append(tag.td(unit,align='right'))
            ret.append(tr)
        if depth>0 or dontRender: return ret
        r1=ret.generate().render('xhtml',encoding='ascii')
        if isinstance(r1,bytes): r1=r1.decode('ascii')
        return r1+u'\n'

SerializerToHtmlTable=SerializerToHtmlTableGenshi


class SerializerToExpr(object):
    '''
    Represent given object as python expression.
    Do not use this class directly, say ``object.dump(format="expr")`` instead.
    '''
    
    unbreakableTypes=(Vector2i,Vector2,Vector3i,Vector3)
    def __init__(self,indent='\t',maxWd=120,noMagic=True):
        self.indent=indent
        self.indentLen=len(indent.replace('\t',3*' '))
        self.maxWd=maxWd
        self.noMagic=noMagic
    def __call__(self,obj,level=0,neededModules=None):
        if neededModules is None: neededModules=set() # instantiate a new one every time
        if isinstance(obj,Object):
            attrs=[(trait.name,getattr(obj,trait.name)) for trait in obj._getAllTraits() if not (trait.hidden or trait.noDump or (trait.hideIf and eval(trait.hideIf,globals(),{'self':obj})))]
            delims=(obj.__class__.__module__)+'.'+obj.__class__.__name__+'(',')'
            neededModules.add(obj.__class__.__module__)
        elif isinstance(obj,dict):
            attrs=list(obj.items())
            delims='{','}'
        # list or mutable list-like objects (NodeList, for instance)
        elif hasattr(obj,'__len__') and hasattr(obj,'__getitem__') and hasattr(obj,'append'):
            attrs=[(None,obj[i]) for i in range(len(obj))]
            delims='[',']'
        # tuple and tuple-like objects: format like tuples
        ## were additionally: Matrix3,Vector6,Matrix6,VectorX,MatrixX,AlignedBox2,AlignedBox3
        elif sum([isinstance(obj,T) for T in (tuple,Vector2i,Vector2,Vector3i,Vector3)])>0:
            attrs=[(None,obj[i]) for i in range(len(obj))]
            delims='(',')'
        # use short representation of float as str
        elif isinstance(obj,float):
            return float2str(obj)
        # don't know what to do, use repr (unhandled or primive types)
        else:
            return repr(obj)
        lst=[(((attr[0]+'=') if attr[0] else '')+self(attr[1],level+1)) for attr in attrs]
        oneLiner=(sum([len(l) for l in lst])+self.indentLen<=self.maxWd or self.maxWd<=0 or type(obj) in self.unbreakableTypes)
        magic=''
        if not self.noMagic and level==0:
            magic='##woo-expression##\n'
            if neededModules: magic+='#: import '+','.join(neededModules)+'\n'
            oneLiner=False
        #magic=('##woo-expression##' if not self.noMagic and level==0 else '')
        if oneLiner: return delims[0]+', '.join(lst)+delims[1]
        indent0,indent1=self.indent*level,self.indent*(level+1)
        return magic+delims[0]+'\n'+indent1+(',\n'+indent1).join(lst)+'\n'+indent0+delims[1]+('\n' if level==0 else '')

# roughly following http://www.doughellmann.com/PyMOTW/json/, thanks!
class WooJSONEncoder(json.JSONEncoder):
    '''
    Represent given object as JSON object.
    Do not use this class directly, say ``object.dump(format="json")`` instead.
    '''
    def __init__(self,indent=3,sort_keys=True,oneway=False):
        'oneway: allow serialization of objects which won\'t be properly deserialized. They are: numpy.ndarray, h5py.Dataset.'
        json.JSONEncoder.__init__(self,sort_keys=sort_keys,indent=indent)
        self.oneway=oneway
    def default(self,obj):
        # Woo objects
        if isinstance(obj,woo.core.Object):
            d={'__class__':obj.__class__.__module__+'.'+obj.__class__.__name__}
            # assign woo attributes
            d.update(dict([(trait.name,getattr(obj,trait.name)) for trait in obj._getAllTraits() if not (trait.hidden or trait.noDump or (trait.hideIf and eval(trait.hideIf,globals(),{'self':obj})))]))
            return d
        # vectors, matrices: those can be assigned from tuples
        elif obj.__class__.__module__=='woo._customConverters' or obj.__class__.__module__=='_customConverters':
            if hasattr(obj,'__len__'): return list(obj)
            else: raise TypeError("Unhandled type for JSON: "+obj.__class__.__module__+'.'+obj.__class__.__name__)
        # minieigen objects
        elif obj.__class__.__module__=='minieigen':
            if isinstance(obj,minieigen.Quaternion): return obj.toAxisAngle()
            else: return tuple(obj[i] for i in range(len(obj)))
        # numpy arrays
        elif isinstance(obj,numpy.ndarray):
            if not self.oneway: raise TypeError('numpy.ndarray can only be serialized with WooJSONEncoder(oneway=True), since deserialization will yield only dict/list, not a numpy.ndarray.')
            # record array: dump as dict of sub-arrays (columns)
            if obj.dtype.names: return dict([(name,obj[name].tolist()) for name in obj.dtype.names])
            # non-record array: dump as nested list
            return obj.tolist()
        # h5py datasets
        elif haveH5py and isinstance(obj,h5py.Dataset):
            if not self.oneway: raise TypeError('h5py.Dataset can only be serialized with WooJSONEncoder(oneway=True), since deserialization will yield only dict/list, not a h5py.Dataset.')
            return obj[...]
        # other types, handled by the json module natively
        else:
            return super(WooJSONEncoder,self).default(obj)

class WooJSONDecoder(json.JSONDecoder):
    '''
    Reconstruct JSON object, possibly containing specially-denoted Woo object (:obj:`WooJSONEncoder`).
    Do not use this class directly, say ``object.loads(format="json")`` instead.

    The *onError* ctor parameter determines what to do with a dictionary defining '__class__', which
    nevertheless cannot be reconstructed properly: this happens when class attributes changed inthe meantime, or
    the class does not exist anymore. Possible values are:

    * ``error``: raise exception (default)
    * ``warn``: log warning and return dictionary
    * ``ignore``: just return dictionary, without any notification
    '''
    def __init__(self,onError='error'):
        assert onError in ('error','warn','ignore')
        json.JSONDecoder.__init__(self,object_hook=self.dictToObject)
        self.onError=onError
    def dictToObject(self,d):
        import sys, woo
        if not '__class__' in d: return d # nothing we know
        klass=d.pop('__class__')
        modPath=klass.split('.')[:-1]
        localns={}
        # import modules as needed so that __class__ can be eval'd
        # stuff the uppermost module to localns
        for i in range(len(modPath)):
            mname='.'.join(modPath[:i+1])
            m=__import__(mname,level=0) # absolute imports only
            if i==0: localns[mname]=m
        try:
            if py3k: return eval(klass,globals(),localns)(**d)
            else: return eval(klass,globals(),localns)(**dict((key.encode('ascii'),(value.encode('ascii') if isinstance(value,str) else value)) for key,value in d.items()))
        except Exception as e:
            if self.onError=='error': raise
            elif self.onError=='warn':
                import logging
                if e.__class__ in (TypeError,AttributeError): reason='class definition changed?'
                elif e.__class__==NameError: reason='class does not exist anymore?'
                else: reason='unknown error'
                logging.warning('%s while decoding class %s from JSON (%s), returning dictionary: %s'%(e.__class__.__name__,klass,reason,str(e)))
            return d

# inject into the core namespace, so that it can be used elsewhere as well
woo.core.WooJSONEncoder=WooJSONEncoder
woo.core.WooJSONDecoder=WooJSONDecoder

# call the arg __e to avoid clash with math.e if there is 'from math import *' in the magic string
def wooExprEval(__e,__f,__overrideHashPercent={}):
    '''
    Evaluate expression created with :obj:`SerializerToExpr`. Comments starting with ``#%`` and ``#:`` are executed as python code before the evaluation happens, which is in particular useful for importing necessary modules. The rule is the following:

    #. Lines starting with ``#%`` are executed in the local context.
    #. Values passed through ``__overrideHashPercent`` override variable values created in the preceding steps (it is an error if some variable is not defined).
    #. Lines starting with ``%:`` are evaluated in the local context.

    The rationale for the 2-step evaluation is that variables defined in ``#%`` can be externally overridden in a controlled manner (if by mistake a non-existent variable is assigned externally, it is an error; if nothing is overridden externally, the default is used); and only after this override, ``#:`` lines can use those variables.

    :param __e: expression to be evaluated
    :param __f: filename (if any) where the expression was stored
    :param __overrideHashPercent: dictionary which will change local variables (defined in ``#%`` lines) before the expression itself is evaluated.
    '''

    import woo,math,textwrap
    # exec all lines starting with #% and #: as a piece of code
    __codePercent=textwrap.dedent('\n'.join([l[2:] for l in __e.split('\n') if l.startswith('#%')]))
    __codeHash   =textwrap.dedent('\n'.join([l[2:] for l in __e.split('\n') if l.startswith('#:')]))
    #print('EXECUTING #: CODE:\n'+__code)
    future.utils.exec_(__codePercent,locals()) # pass locals() here
    for __var,__val in __overrideHashPercent.items():
       if __var not in locals(): raise NameError("Local (defined in #%%) variable '%s' does not exist, unable to override its value '%s' from __overrideHashPercent."%(__var,str(__val)))
       locals()[__var]=__val
    # re-evaluate the block with locals from overrideHashPercent
    future.utils.exec_(__codeHash,locals()) # pass locals() here
    # return the expression
    return eval(compile(__e,__f,'eval'),locals(),globals())

def Object_loads(typ,data,format='auto',overrideHashPercent={}):
    'Load object from file, with format auto-detection; when *typ* is None, no type-checking is performed.'
    def typeChecked(obj,type):
        if type==None: return obj
        if not isinstance(obj,typ): raise TypeError('Loaded object of type '+obj.__class__.__name__+' is not a '+typ.__name__)
        return obj
    if format not in ('auto','pickle','expr','json'): raise ValueError('Invalid format %s'%format)
    elif format=='auto':
        if type(data)==bytes and data.startswith(b'##woo-expression##'): format='expr'
        elif type(data) in (str,past.builtins.str) and data.startswith('##woo-expression##'): format='expr'
        else:
            # try pickle
            try:
                if py3k and type(data)==str: return typeChecked(pickle.loads(bytes(data,'utf-8')),typ)
                else: return typeChecked(pickle.loads(data),typ)
            except (IOError,KeyError,pickle.UnpicklingError,EOFError): pass
            # try json
            try: return typeChecked(WooJSONDecoder().decode(data),typ)
            except (IOError,ValueError,KeyError): pass
    if format=='auto': IOError("Format detection failed on data: "%data)
    if overrideHashPercent and format!='expr': raise ValueError("overrideHashPercent only applicable with the 'expr' format (not '%s')."%format)
    ## format detected now
    if format=='expr':
        return typeChecked(wooExprEval(data,'<string>',__overrideHashPercent=overrideHashPercent),typ)
    elif format=='pickle':
        return typeChecked(pickle.loads(data,typ))
    elif format=='json':
        return typeChecked(WooJSONDecoder().decode(data),typ)
    assert False # impossible


def Object_load(typ,inFile,format='auto',overrideHashPercent={}):
    def typeChecked(obj,type):
        if type==None: return obj
        if not isinstance(obj,typ): raise TypeError('Loaded object of type '+obj.__class__.__name__+' is not a '+typ.__name__)
        return obj
    validFormats=('auto','boost::serialization','expr','pickle','json')
    if format not in validFormats: raise ValueError('format must be one of '+', '.join(validFormats)+'.')
    if format=='auto':
        format=None
        ## DO NOT use extensions to determine type, that is evil
        # check for compression first
        head=open(inFile,'rb').read(100)
        # we might want to pass this data to ObjectIO, which currently determines format by looking at extension
        if head[:2]==b'\x1f\x8b':
            import gzip
            head=gzip.open(inFile,'rb').read(100)
        elif head[:2]==b'BZ':
            import bz2
            head=bz2.BZ2File(inFile,'rb').read(100)
        # detect real format here (head is uncompressed already)
        # the string between nulls is 'serialization::archive'
        # see http://stackoverflow.com/questions/10614215/magic-for-detecting-boostserialization-file
        # newer versions (1.51 and perhaps greater) put '\n' (\x0a) after serialization::archive instead of null,
        # so let's just not test the byte after "archive"
        if head.startswith(b'\x16\x00\x00\x00\x00\x00\x00\x00\x73\x65\x72\x69\x61\x6c\x69\x7a\x61\x74\x69\x6f\x6e\x3a\x3a\x61\x72\x63\x68\x69\x76\x65'):
            format='boost::serialization'
        elif head.startswith(b'<?xml version="1.0"'):
            format='boost::serialization'
        elif head.startswith(b'##woo-expression##'):
            format='expr'
        else:
            # test pickling by trying to load
            try: return typeChecked(pickle.load(open(inFile,'rb')),typ) # open again to seek to the beginning
            except (IOError,KeyError,pickle.UnpicklingError,EOFError): pass
            try: return typeChecked(WooJSONDecoder().decode(codecs.open(inFile,'rb','utf-8').read()),typ)
            except (IOError,ValueError): pass
        if not format:    raise RuntimeError('File format detection failed on %s (head: %s, bin: %s)'%(inFile,''.join(["\\x%02x"%(x if py3k else ord(x)) for x in head]),str(head))) # in py3k, bytes contain integers rather than chars
    if format not in validFormats: raise RuntimeError("format='%s'??"%format)
    assert format in validFormats
    if overrideHashPercent and format!='expr': raise ValueError("overrideHashPercent only applicable with the 'expr' format (not '%s')"%format)
    if format==None:
        raise IOError('Input file format not detected')
    elif format=='boost::serialization':
        # ObjectIO takes care of detecting binary, xml, compression independently
        return typeChecked(Object._boostLoad(str(inFile)),typ) # convert unicode to str, if necessary, as the c++ type is std::string
    elif format=='expr':
        buf=codecs.open(inFile,'rb','utf-8').read()
        return typeChecked(wooExprEval(buf,inFile,__overrideHashPercent=overrideHashPercent),typ)
    elif format=='pickle':
        return typeChecked(pickle.load(open(inFile,'rb')),typ)
    elif format=='json':
        return typeChecked(WooJSONDecoder().decode(codecs.open(inFile,'rb','utf-8').read()),typ)
    assert False



def Object_loadTmp(typ,name=''):
    obj=woo.master.loadTmpAny(str(name))
    if not isinstance(obj,typ): raise TypeError('Loaded object of type '+obj.__class__.__name__+' is not a '+typ.__name__)
    return obj
def Object_saveTmp(obj,name='',quiet=False):
    woo.master.saveTmpAny(obj,name,quiet)
def Object_deepcopy(obj,**kw):
    'Make object deepcopy by serializing to memory and deserializing.'
    return woo.master.deepcopy(obj,**kw)

Object._getAllTraits=Object_getAllTraits
Object._getAllTraitsWithClasses=Object_getAllTraitsWithClasses
Object.dump=Object_dump
Object.dumps=Object_dumps
Object.saveTmp=Object_saveTmp
Object.deepcopy=Object_deepcopy
Object.load=classmethod(Object_load)
Object.loads=classmethod(Object_loads)
Object.loadTmp=classmethod(Object_loadTmp)


def Master_save(o,*args,**kw):
    o.scene.save(*args,**kw)
def Master_load(o,*args,**kw):
    o.scene=woo.core.Scene.load(*args,**kw)
def Master_reload(o,quiet=None,*args,**kw): # this arg is deprecated
    'Reload master scene, using its :obj:`woo.core.Scene.lastSave`; assigns ``woo.master.scene`` and returns the new scene object'
    f=o.scene.lastSave
    if not f: raise ValueError("Scene.lastSave is empty.")
    if f.startswith(':memory:'): o.scene=woo.core.Scene.loadTmp(f[8:])
    else: o.scene=woo.core.Scene.load(f,*args,**kw)
    return o.scene
def Master_loadTmp(o,name='',quiet=None): # quiet deprecated
    'Load scene from temporary storage, assign to it ``woo.master.scene`` and return it.'
    o.scene=woo.core.Scene.loadTmp(name)
    return o.scene
def Master_saveTmp(o,name='',quiet=False):
    o.scene.lastSave=':memory:'+name
    o.scene.saveTmp(name,quiet)

woo.core.Master.save=Master_save
woo.core.Master.load=Master_load
woo.core.Master.reload=Master_reload
woo.core.Master.loadTmp=Master_loadTmp
woo.core.Master.saveTmp=Master_saveTmp

def Master_run(o,*args,**kw): return o.scene.run(*args,**kw)
def Master_pause(o,*args,**kw): return o.scene.stop(*args,**kw)
def Master_step(o,*args,**kw): return o.scene.one(*args,**kw)
def Master_wait(o,*args,**kw): return o.scene.wait(*args,**kw)
def Master_reset(o): o.scene=woo.core.Scene()

#def Master_running(o.,*args,**kw): return o.scene.running
woo.core.Master.run=Master_run
woo.core.Master.pause=Master_pause
woo.core.Master.step=Master_step
woo.core.Master.wait=Master_wait
woo.core.Master.running=property(lambda o: o.scene.running)
woo.core.Master.reset=Master_reset
