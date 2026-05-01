from pathlib import Path
from shutil import copytree, rmtree, ignore_patterns
import os, sys, getopt

# this rather robust and obtuse tasks script is only for the CLI
# DO NOT USE this as baseline for any other packages

_post_install = 'postinstall'
_post_uninstall = 'postuninstall'
_error_log = f'No valid post arg, nothing will run. Expecting "{_post_install}" or "{_post_uninstall}"'

def main(argv):
    if len(argv) != 1:
        print (_error_log)
        return

    argv = argv[0].lower()
    if argv == _post_install:
        post_install()
    elif argv == _post_uninstall:
        post_uninstall()
    else:
        print (_error_log)

def post_install():
    package = Path.cwd()
    project_dir = node_modules_parent()
    
    if (project_dir != None):
        name = package.name.rsplit('.', 1)[-1]
        destination = project_dir.joinpath('mdk', 'mdk-cli')
        print(f'copy {package} to {destination}')
        if destination.exists():
            print("Package already exists in mdk. Please delete the directory before installing again.")
            return
        copytree(package, destination, ignore=ignore_patterns('package.json', 'tasks.py'))
        
def node_modules_parent() -> Path:
    parent_dir = Path('../..')
    node_modules = parent_dir.joinpath('node_modules').resolve()
    if not node_modules.is_dir():
        print(f'cannot find node_modules folder at {parent_dir.resolve()}')
    return parent_dir.resolve()
    

def post_uninstall():
    package = Path.cwd()
    project_dir = node_modules_parent()
    
    if (project_dir != None):
        #Removing directory created during install
        name = package.name.rsplit('.', 1)[-1]
        destination = project_dir.joinpath('mdk', 'mdk-cli')
        print(f'Deleting {name} from {destination}')
        if destination.exists():
            rmtree(destination)
        
if __name__ == "__main__":
   main(sys.argv[1:])