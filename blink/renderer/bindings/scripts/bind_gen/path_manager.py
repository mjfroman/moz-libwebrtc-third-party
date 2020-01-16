# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import posixpath

import web_idl

from . import name_style
from .blink_v8_bridge import blink_class_name


class PathManager(object):
    """
    Provides a variety of paths such as Blink headers and output files.  Unless
    explicitly specified, returned paths are relative to the project's root
    directory or the root directory of generated files.
    e.g. "third_party/blink/renderer/..."

    About output files, there are two cases.
    - cross-components case:
        APIs are generated in 'core' and implementations are generated in
        'modules'.
    - single component case:
        Everything is generated in a single component.
    """

    _REQUIRE_INIT_MESSAGE = ("PathManager.init must be called in advance.")
    _is_initialized = False

    @classmethod
    def init(cls, root_src_dir, root_gen_dir, component_reldirs):
        """
        Args:
            root_src_dir: Project's root directory, which corresponds to "//"
                in GN.
            root_gen_dir: Root directory of generated files, which corresponds
                to "//out/Default/gen" in GN.
            component_reldirs: Pairs of component and output directory relative
                to |root_gen_dir|.
        """
        assert not cls._is_initialized
        assert isinstance(root_src_dir, str)
        assert isinstance(root_gen_dir, str)
        assert isinstance(component_reldirs, dict)

        cls._blink_path_prefix = posixpath.sep + posixpath.join(
            "third_party", "blink", "renderer", "")

        cls._root_src_dir = posixpath.abspath(root_src_dir)
        cls._root_gen_dir = posixpath.abspath(root_gen_dir)
        cls._component_reldirs = {
            component: posixpath.normpath(rel_dir)
            for component, rel_dir in component_reldirs.iteritems()
        }
        cls._is_initialized = True

    @staticmethod
    def gen_path_to(path):
        """
        Returns the absolute path of |path| that must be relative to the root
        directory of generated files.
        """
        assert PathManager._is_initialized, PathManager._REQUIRE_INIT_MESSAGE
        return posixpath.abspath(
            posixpath.join(PathManager._root_gen_dir, path))

    @classmethod
    def relpath_to_project_root(cls, path):
        index = path.find(cls._blink_path_prefix)
        if index < 0:
            assert path.startswith(cls._blink_path_prefix[1:])
            return path
        return path[index + 1:]

    def __init__(self, idl_definition):
        assert self._is_initialized, self._REQUIRE_INIT_MESSAGE

        if isinstance(idl_definition, web_idl.Dictionary):
            idl_path = PathManager.relpath_to_project_root(
                posixpath.normpath(
                    idl_definition.debug_info.location.filepath))
            idl_basepath, _ = posixpath.splitext(idl_path)
            self._idl_dir, _ = posixpath.split(idl_basepath)
        else:
            self._idl_dir = None

        components = sorted(idl_definition.components)  # "core" < "modules"

        if len(components) == 0:
            assert isinstance(idl_definition, web_idl.Union)
            # Unions of built-in types, e.g. DoubleOrString, do not have a
            # component.
            self._is_cross_components = False
            default_component = web_idl.Component("core")
            self._api_component = default_component
            self._impl_component = default_component
        if len(components) == 1:
            component = components[0]
            self._is_cross_components = False
            self._api_component = component
            self._impl_component = component
        elif len(components) == 2:
            assert components[0] == "core"
            assert components[1] == "modules"
            self._is_cross_components = True
            self._api_component = components[0]
            self._impl_component = components[1]
        else:
            assert False

        self._api_dir = self._component_reldirs[self._api_component]
        self._impl_dir = self._component_reldirs[self._impl_component]
        self._v8_bind_basename = name_style.file("v8",
                                                 idl_definition.identifier)
        self._blink_basename = name_style.file(
            blink_class_name(idl_definition))

    @property
    def is_cross_components(self):
        return self._is_cross_components

    @property
    def api_component(self):
        return self._api_component

    @property
    def api_dir(self):
        return self._api_dir

    def api_path(self, filename=None, ext=None):
        return self._join(
            dirpath=self.api_dir,
            filename=(filename or self._v8_bind_basename),
            ext=ext)

    @property
    def impl_component(self):
        return self._impl_component

    @property
    def impl_dir(self):
        return self._impl_dir

    def impl_path(self, filename=None, ext=None):
        return self._join(
            dirpath=self.impl_dir,
            filename=(filename or self._v8_bind_basename),
            ext=ext)

    # TODO(crbug.com/1034398): Remove this API
    def dict_path(self, filename=None, ext=None):
        assert self._idl_dir is not None
        return self._join(
            dirpath=self._idl_dir,
            filename=(filename or self._blink_basename),
            ext=ext)

    @staticmethod
    def _join(dirpath, filename, ext=None):
        if ext is not None:
            filename = posixpath.extsep.join([filename, ext])
        return posixpath.join(dirpath, filename)
