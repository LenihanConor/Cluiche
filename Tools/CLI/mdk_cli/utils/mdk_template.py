from box import Box
from jinja2 import Environment, FileSystemLoader
import json
from loguru import logger
from pathlib import Path
from string import Template

## @addtogroup utils_grp
#  @{

## @package mdk_template Template and code-generation utilities.


class CodeGenTemplate:
    """!Generates code from a tree structure template and Jinja template files.
    See [Code Gen Docs](http://localhost:1313/project-mdk-documentation/mdk-cli/mdk-cli-how-to-guides/mdk-cli-codegen/) for details. # noqa
    """

    def __init__(
            self,
            templates_folder_path,
            tree_template_filename='tree_template.json',
            template_params=None,
            dest_folder=Path.cwd,
            **kwargs):
        """!
        @param templates_folder_path Required. The `pathlib.Path` to the folder containing tree and source code templates. 
        @param tree_template_filename string filename of template file defining a tree structure. Should be in templates folder. # noqa
        @param template_params `dict` containing template replacement parameters. Will be passed to the tree template and Jinja template.
        @param dest_folder the root `pathlib.Path` where the generated tree will be generated.
        """
        self.templates_folder_path = templates_folder_path
        self.tree_template_path = templates_folder_path.joinpath(tree_template_filename).resolve()
        self.templates_folder_path = templates_folder_path
        file_loader = FileSystemLoader(str(self.templates_folder_path))
        self.environment = Environment(loader=file_loader)
        if template_params is None:
            self.template_params = {}
        else:
            self.template_params = template_params
        self.dest_folder = dest_folder

    def generate(self):
        """!Generates code based on template definitions passed in __init__()"""
        logger.debug(f"Loading tree template from {self.tree_template_path}")
        with open(str(self.tree_template_path)) as f:
            tree = Box(json.load(f))
        self._process_folder(
            folder=tree,
            params=self.template_params,
            current_path=self.dest_folder)

    def _process_folder(self, folder, params, current_path):
        """Recursively processes a folder:
        1. Creates the folder if necessary
        2. Generates code using templates for all file definitions in this folder
        """
        for f in folder.folders:
            logger.debug(f"Processing folder '{f.name}'. current_path = {str(current_path)}")
            new_folder = current_path.joinpath(f.name).resolve()
            new_folder.mkdir(parents=True, exist_ok=True)
            self._process_folder(f, params, new_folder)
        if hasattr(folder, "files"):
            for file_definition in folder.files:
                logger.debug(f"Processing file {file_definition.filename}, ")
                (f"template: {file_definition.template}")
                self._create_file_from_template(file_definition, params, current_path)

    def _create_file_from_template(self, file_definition, params, dest_folder):
        """Performs actual code generation for a given file."""
        template_file = file_definition.template
        target_filename = Template(file_definition.filename).substitute(params)
        template = self.environment.get_template(template_file)
        code = template.render(params)
        dest_path = dest_folder.joinpath(target_filename).resolve()
        logger.debug(f"Creating {str(dest_path)} from template: {template_file}")
        dest_path.write_text(code)

## @}
