# generate_swiglal_iface.py - generate SWIG interface
#
# Copyright (C) 2011, 2012 Karl Wette
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with with program; see the file COPYING. If not, write to the
# Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
# MA  02111-1307  USA

__author__ = 'Karl Wette <karl.wette@ligo.org>'
__copyright__ = 'Copyright (C) 2011, 2012 Karl Wette'

import sys, os, re
import xml.parsers.expat

_, my_basename = os.path.split(__file__)

# get input variables from environment
module_name = os.environ['PACKAGE_NAME']
module_depends = os.environ['SWIG_MODULE_DEPENDS']
symbol_prefixes = os.environ['SWIG_SYMBOL_PREFIXES']
preproc_filename = os.environ['swig_iface_preproc']
preproc_xml_filename = os.environ['swig_iface_preproc_xml']
iface_filename = os.environ['swig_iface']

# print error message and exit
def fail(msg):
    sys.stderr.write('%s: %s\n' % (my_basename, msg))
    sys.exit(1)

# XML parser class
class XMLParser:

    # create parser
    def __init__(self):
        self.parser = xml.parsers.expat.ParserCreate()
        self.parser.StartElementHandler = self.start_element
        self.parser.EndElementHandler = self.end_element
        self.element_stack = []
        self.action_stack = []

    # parse file, starting with data and initial action
    def parse(self, filename, action):
        self.action_stack.append(action)
        file = open(filename, 'r')
        self.parser.ParseFile(file)
        file.close()

    # handler for opening element tag:
    # store current element in stack, call action, then set action
    # stack to returned action for the top 'level' number of elements
    def start_element(self, name, attrs):
        self.element_stack.append(name)
        action, level = self.action_stack[-1].start(self.element_stack, attrs)
        for _ in range(level-1):
            self.action_stack.pop()
        for _ in range(level):
            self.action_stack.append(action)

    # handler for closing element tag:
    # pop element and action stacks
    def end_element(self, name):
        assert(self.element_stack[-1] == name)
        self.action_stack[-1].end(self.element_stack)
        self.element_stack.pop()
        self.action_stack.pop()

# find the top-level <include> element:
# found if 'name' attribute equals SWIG interface name
class FindTopLevelInclude:
    def __init__(self, headers):
        self.headers = headers
    def start(self, elems, attrs):
        if len(elems) >= 3:
            if elems[-3] == 'include':
                if elems[-2] == 'attributelist':
                    if elems[-1] == 'attribute':
                        if attrs.get('name') == 'name':
                            if attrs.get('value') == preproc_filename:
                                return (FindHeaderInclude(self.headers), 3)
        return (self, 1)
    def end(self, elems):
        pass

# find <include> elements for each header:
# found if 'name' attribute equals name of header file
class FindHeaderInclude:
    def __init__(self, headers):
        self.headers = headers
    def start(self, elems, attrs):
        if len(elems) >= 3:
            if elems[-3] == 'include':
                if elems[-2] == 'attributelist':
                    if elems[-1] == 'attribute':
                        if attrs.get('name') == 'name':
                            filepath, filename = os.path.split(attrs.get('value'))
                            _, filedir = os.path.split(filepath)
                            filename = os.path.join(filedir, filename)
                            return (ParseHeader(self.headers, filename), 3)
        return (self, 1)
    def end(self, elems):
        pass

# parse header elements
class ParseHeader:
    def __init__(self, headers, filename):
        self.headers = headers
        self.filename = filename
        self.structs = []
        self.cdecls = []
    def start(self, elems, attrs):
        if len(elems) >= 1:
            if elems[-1] == 'include':
                return (FindHeaderInclude(self.headers), 1)
            elif elems[-1] == 'class':
                self.structs.append({})
                return (ParseStruct(self.structs), 1)
            elif elems[-1] == 'cdecl':
                self.cdecls.append({})
                return (ParseCDecl(self.cdecls), 1)
        return (self, 1)
    def end(self, elems):
        if len(elems) >= 1:
            if elems[-1] == 'include':
                header = {
                    'index' : len(self.headers),
                    'struct' : self.structs,
                    'cdecl' : self.cdecls
                    }
                self.headers[self.filename] = header
        pass

# parse a struct
class ParseStruct:
    def __init__(self, structs):
        self.structs = structs
    def start(self, elems, attrs):
        if len(elems) >= 3:
            if elems[-3] == 'class':
                if elems[-2] == 'attributelist':
                    if elems[-1] == 'attribute':
                        self.structs[-1][attrs.get('name')] = attrs.get('value')
        return (self, 1)
    def end(self, elems):
        pass

# parse a function prototype
class ParseCDecl:
    def __init__(self, cdecls):
        self.cdecls = cdecls
    def start(self, elems, attrs):
        if len(elems) >= 3:
            if elems[-3] == 'cdecl':
                if elems[-2] == 'attributelist':
                    if elems[-1] == 'attribute':
                        self.cdecls[-1][attrs.get('name')] = attrs.get('value')
        return (self, 1)
    def end(self, elems):
        pass

# parse SWIG parse tree of preprocessing interface file
xml_parser = XMLParser()
headers = {}
xml_parser.parse(preproc_xml_filename, FindTopLevelInclude(headers))

# always include SWIG interfaces after C headers
for header in headers:
    if header.endswith('.i'):
        headers[header]['index'] += len(headers)

# order headers so that headers appear before headers than #include them
header_files = sorted(headers, key=lambda h : headers[h]['index'])

# process symbols from interface header parse trees
functions = {}
structs = {}
tdstructs = {}
clear_macros = {}
for header in headers:
    clear_macros[header] = set()

    # C declarations
    for cdecl in headers[header]['cdecl']:
        if not 'name' in cdecl:
            fail("cdecl in header '%s' has no name" % header)
        if not 'kind' in cdecl:
            fail("cdecl '%s' in header '%s' has no kind" % (cdecl['name'], header))
        if not 'type' in cdecl or cdecl['type'] == '':
            fail("cdecl '%s' in header '%s' has no type" % (cdecl['name'], header))
        cdecl['extra_process_args'] = []

        # SWIGLAL() and SWIGLAL_CLEAR() macros
        if cdecl['name'] in ['__swiglal__', '__swiglal_clear__']:
            if not 'value' in cdecl:
                fail("cdecl '%s' in header '%s' has no value" % (cdecl['name'], header))
            macro = re.sub(r'\s', '', cdecl['value'])
            if cdecl['name'] == '__swiglal__':
                if macro in clear_macros[header]:
                    fail("duplicate definition of SWIGLAL(%s)", macro)
                else:
                    clear_macros[header].add(macro)
            else:
                if macro in clear_macros[header]:
                    clear_macros[header].remove(macro)
                else:
                    fail("cannot clear undefined macro SWIGLAL(%s)", macro)

        # functions
        elif cdecl['kind'] == 'function':
            if cdecl['name'] in functions:
                fail("duplicate function '%s' in header '%s'" % (cdecl['name'], header))
            functions[cdecl['name']] = cdecl

        # typedefs to structs
        elif cdecl['kind'] == 'typedef' and cdecl['type'].startswith('struct '):
            if cdecl['name'] in tdstructs:
                fail("duplicate struct typedef '%s' in header '%s'" % (cdecl['name'], header))
            cdecl['tagname'] = cdecl['type'][7:]
            tdstructs[cdecl['name']] = cdecl

    # structs
    for struct in headers[header]['struct']:
        if not 'name' in struct:
            fail("struct in header '%s' has no name" % header)
        if 'unnamed' in struct:
            fail("struct '%s' in header '%s' has no tag-name" % (struct['name'], header))
        if struct['name'] in structs:
            fail("duplicate struct '%s' in header %s" % (struct['name'], header))
        struct['extra_process_args'] = []
        structs[struct['name']] = struct

# look for a destructor function for each struct
dtor_decl_regexp = re.compile('^f\(p\.(.*)\)\.$')
for function_name in functions:

    # function must contain 'Destroy' in name, and return void
    if not 'Destroy' in function_name:
        continue
    if not functions[function_name]['type'] == 'void':
        continue

    # function must take a single pointer argument
    dtor_decl = functions[function_name]['decl']
    dtor_decl_match = dtor_decl_regexp.match(dtor_decl)
    if dtor_decl_match is None:
        continue
    dtor_struct_name = dtor_decl_match.group(1)

    # function argument must be a struct name
    if not dtor_struct_name in tdstructs:
        continue

    # save destructor name
    tdstructs[dtor_struct_name]['dtor_function'] = function_name

    # remove destructor function from interface
    functions[function_name]['feature_ignore'] = '1'

# determine whether return value of functions should be ignored
func_arg_types_regexp = re.compile('^f\((.*)\)\.(p\.)?$')
for function_name in functions:

    # get function argument and return types
    func_decl = functions[function_name]['decl']
    func_arg_types_match = func_arg_types_regexp.match(func_decl)
    if func_arg_types_match is None:
        fail("could not match function declaration '%s'" % func_decl)
    func_arg_types = func_arg_types_match.group(1).split(',')
    func_retn_type = functions[function_name]['type']
    if not func_arg_types_match.group(2) is None:
        func_retn_type = func_arg_types_match.group(2) + func_retn_type

    # return function values by default
    func_ignore_retn = False

    # ignore function return values whose type is 'int',
    # since this is interpreted as a XLAL error code
    if func_retn_type == 'int':
        func_ignore_retn = True

    # ignore function return values whose type is a pointer and which
    # matches the type of the first argument, since it is common for
    # XLAL functions to return the value of the first argument
    if func_retn_type.startswith('p.') and func_retn_type == func_arg_types[0]:
        func_ignore_retn = True

    if func_ignore_retn:

        # construct return type from SWIG type syntax
        if func_retn_type.startswith('p.'):
            func_c_retn_type = func_retn_type[2:] + '*'
        else:
            func_c_retn_type = func_retn_type
        func_c_retn_type = func_c_retn_type.replace('q(const).', 'const ')

    else:

        # empty return type indicates that return value is not ignored
        func_c_retn_type = ''

    # add return type as extra argument to swiglal_process_function() macro
    functions[function_name]['extra_process_args'].append(func_c_retn_type)

# open SWIG interface file
iface_file = open(iface_filename, 'w')
_, iface_file_basename = os.path.split(iface_filename)
iface_file.write('// %s: generated by %s\n' % (iface_file_basename, my_basename))

# define module name
iface_file.write('%%module %s;\n' % module_name)

# include common interface code
iface_file.write('%include <lal/swiglal_common.i>\n')

# import dependent modules
for module in module_depends.split():
    iface_file.write('%%import <lal/%sswig.i>\n' % module)

# include interface headers in wrapping code
iface_file.write('%header %{\n')
for header in header_files:
    iface_file.write('#include <%s>\n' % header)
iface_file.write('%}\n')

# process interface symbols, with renaming
symbol_prefix_list = symbol_prefixes.split()
symbol_prefix_list.append('')
symbols_to_process = (
    (functions, 'function', 'name'),
    (tdstructs, 'tdstruct', 'tagname')
    )
for (symbols, symbol_type, symbol_name_key) in symbols_to_process:

    # rank symbol by where their prefix comes in prefix list
    for symbol_key in symbols:
        for rank in range(len(symbol_prefix_list)):
            if symbol_key.startswith(symbol_prefix_list[rank]):
                symbols[symbol_key]['rename_rank'] = rank
                break

    # iterate over symbols in order of decreasing rank
    symbol_renames = {}
    sort_by_rank_then_name = lambda k : '%1d%s' % (symbols[k]['rename_rank'], k)
    for symbol_key in sorted(symbols, key=sort_by_rank_then_name):

        # get name of symbol
        symbol_name = symbols[symbol_key][symbol_name_key]

        # strip prefix from symbol key to get rename
        rank = symbols[symbol_key]['rename_rank']
        symbol_rename = symbol_key[len(symbol_prefix_list[rank]):]

        # ignore symbol if another symbol has been renamed to this symbol,
        # otherwise record that a symbol has been renamed to this rename
        if symbol_rename in symbol_renames:
            symbol_rename = '$ignore'
        else:
            symbol_renames[symbol_rename] = True

        #  ignore symbol if the 'feature_ignore' attribute has been set
        if 'feature_ignore' in symbols[symbol_key]:
            symbol_rename = '$ignore'

        # get macro arguments
        macro_args = [symbol_name, symbol_rename]
        macro_args.extend(symbols[symbol_key]['extra_process_args'])

        # write to interface file
        iface_file.write('%%swiglal_process_%s(%s);\n' % (symbol_type, ', '.join(macro_args)))

# include interface headers, and clear SWIGLAL() macros afterwards
for header in header_files:
    iface_file.write('%%include <%s>\n' % header)
    for macro in clear_macros[header]:
        iface_file.write('SWIGLAL_CLEAR(%s);\n' % macro)

# generate constructors and destructors for structs
for struct_name in sorted(tdstructs):
    struct_tagname = tdstructs[struct_name]['tagname']
    struct_opaque = not struct_tagname in structs
    if 'dtor_function' in tdstructs[struct_name]:
        struct_dtor_function = tdstructs[struct_name]['dtor_function']
    else:
        struct_dtor_function = ''
    iface_file.write('%%swiglal_generate_struct_cdtor(%s, %s, %i, %s)\n' % (struct_name, struct_tagname, struct_opaque, struct_dtor_function))

# close SWIG interface file
iface_file.close()