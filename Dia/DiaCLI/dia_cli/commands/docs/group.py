"""'dia docs' Click group — deterministic documentation management commands."""
import click

from .capabilities_cmd import capabilities
from .plan_cmd import plan
from .registry_cmd import registry
from .spec_done_cmd import spec_done
from .vcxproj_cmd import vcxproj_add
from .backlog_cmd import backlog
from .spec_scaffold_cmd import spec_scaffold
from .test_scaffold_cmd import test_scaffold
from .precommit_cmd import precommit


@click.group("docs")
def docs_group():
    """Deterministic documentation and plan management (zero AI tokens)."""


docs_group.add_command(capabilities)
docs_group.add_command(plan)
docs_group.add_command(registry)
docs_group.add_command(spec_done, "spec-done")
docs_group.add_command(vcxproj_add, "vcxproj-add")
docs_group.add_command(backlog)
docs_group.add_command(spec_scaffold, "spec-scaffold")
docs_group.add_command(test_scaffold, "test-scaffold")
docs_group.add_command(precommit)
