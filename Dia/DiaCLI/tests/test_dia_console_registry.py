"""Unit tests for dia_console/registry.py (console-command-model feature).

Covers reflection of the real DiaCLI Click tree (Goal 1), argument-type
mapping with the degrade-to-STRING fallback (Goal 2), and no-cache
reflection (Goal 7 / SD-CONSOLE-010).
"""
import click
import pytest

from dia_cli.cli_main import cli as dia_cli_app
from dia_console.model import ArgumentType, CommandDescriptor
from dia_console.registry import CommandRegistry


# ---------------------------------------------------------------------------
# Independent reference walk of the Click tree (the "golden list")
#
# Deliberately written separately from registry.py so that a bug in one is not
# mirrored by the other, and so newly added DiaCLI commands flow into the
# expectation automatically instead of breaking a hardcoded count.
# ---------------------------------------------------------------------------

def _reference_leaf_ids(command, path=()):
    ids = []
    for name in command.list_commands(click.Context(command)):
        sub = command.get_command(click.Context(command), name)
        if sub is None:
            continue
        if isinstance(sub, click.MultiCommand):
            ids.extend(_reference_leaf_ids(sub, path + (name,)))
        else:
            ids.append(".".join(path + (name,)))
    return ids


@pytest.fixture(scope="module")
def real_registry():
    return CommandRegistry.from_click_app(dia_cli_app)


# ---------------------------------------------------------------------------
# Goal 1 — every command/subcommand is reflected with a correct dotted id
# ---------------------------------------------------------------------------

def test_registry_matches_reference_walk_of_real_cli(real_registry):
    """AC: a CommandDescriptor exists for every currently-registered command."""
    assert set(real_registry.ids()) == set(_reference_leaf_ids(dia_cli_app))


def test_registry_contains_known_nested_commands(real_registry):
    """Sanity anchors at depth 1, 2 and 3 — guards against both walks being wrong."""
    ids = set(real_registry.ids())
    for expected in ("run", "launch", "asset.build", "test.googletest",
                     "check.arch", "docs.plan", "env.setup", "scaffold.stage",
                     "pipeline.deploy.cluichetest"):
        assert expected in ids, f"{expected} missing from reflected registry"


def test_groups_are_not_registered_as_runnable_commands(real_registry):
    """Groups are navigation, not executables — only leaves get descriptors."""
    ids = set(real_registry.ids())
    for group in ("test", "check", "asset", "pipeline", "pipeline.deploy", "env.docker"):
        assert group not in ids


def test_ids_are_unique(real_registry):
    assert len(real_registry.ids()) == len(set(real_registry.ids()))


def test_id_path_and_category_are_consistent(real_registry):
    for descriptor in real_registry:
        assert descriptor.path == tuple(descriptor.id.split("."))
        assert descriptor.name == descriptor.path[-1]
        assert descriptor.category == ".".join(descriptor.path[:-1])


def test_top_level_command_has_empty_category(real_registry):
    assert real_registry.get("run").category == ""


def test_nested_command_category_is_parent_dotted_path(real_registry):
    assert real_registry.get("pipeline.deploy.cluichetest").category == "pipeline.deploy"


def test_get_returns_none_for_unknown_id(real_registry):
    assert real_registry.get("no.such.command") is None


def test_require_raises_for_unknown_id(real_registry):
    with pytest.raises(KeyError):
        real_registry.require("no.such.command")


def test_reflection_produced_no_errors(real_registry):
    assert real_registry.reflection_errors == ()


def test_descriptor_carries_description(real_registry):
    assert real_registry.get("asset.build").description.startswith("Run the full asset pipeline")


# ---------------------------------------------------------------------------
# Goal 2 — every param gets a valid ArgumentType, never an exception
# ---------------------------------------------------------------------------

def test_every_param_of_every_real_command_has_a_valid_type(real_registry):
    """AC: no exception is raised for any Click param type in the current tree."""
    for descriptor in real_registry:
        for param in (*descriptor.arguments, *descriptor.options):
            assert isinstance(param.type, ArgumentType)
            assert param.raw_click_type != ""


def test_choice_option_populates_choices(real_registry):
    config = next(o for o in real_registry.get("asset.build").options if o.name == "config")
    assert config.type is ArgumentType.CHOICE
    assert config.choices == ("Debug", "Release")


def test_path_argument_maps_to_path(real_registry):
    plan_path = next(a for a in real_registry.get("docs.plan").arguments if a.name == "plan_path")
    assert plan_path.type is ArgumentType.PATH
    assert plan_path.required is True


def test_flag_option_maps_to_bool(real_registry):
    force = next(o for o in real_registry.get("asset.build").options if o.name == "force")
    assert force.is_flag is True
    assert force.type is ArgumentType.BOOL
    assert force.default is False


def test_unprocessed_variadic_argument_degrades_to_string(real_registry):
    """click.UNPROCESSED is outside the closed enum: STRING + raw type preserved."""
    args = next(a for a in real_registry.get("api.exec").arguments if a.name == "args")
    assert args.type is ArgumentType.STRING
    assert args.raw_click_type == "UnprocessedParamType"
    assert args.multiple is True  # nargs=-1


def test_option_cli_flag_is_the_real_flag_not_the_param_name(real_registry):
    """DiaCLI renames many params (--module -> module_filter); argv needs the real flag."""
    module_filter = next(o for o in real_registry.get("check.arch").options if o.name == "module_filter")
    assert module_filter.cli_flag == "--module"


# ---------------------------------------------------------------------------
# Type mapping against a synthetic tree (exhaustive, deterministic)
# ---------------------------------------------------------------------------

class _CustomType(click.ParamType):
    name = "custom"

    def convert(self, value, param, ctx):  # pragma: no cover - never invoked here
        return value


@pytest.fixture
def synthetic_app():
    @click.group()
    def root():
        """Root group."""

    @root.command("types")
    @click.option("--text", type=str, default="t")
    @click.option("--count", type=int, default=1)
    @click.option("--ratio", type=float, default=0.5)
    @click.option("--toggle", is_flag=True, default=False)
    @click.option("--where", type=click.Path(), default=None)
    @click.option("--mode", type=click.Choice(["a", "b"]), default="a")
    @click.option("--ranged", type=click.IntRange(0, 10), default=5)
    @click.option("--weird", type=_CustomType(), default=None)
    @click.option("--boolish", type=bool, default=False)
    def types_cmd(**kwargs):
        """Type coverage."""

    @root.group("outer")
    def outer():
        """Outer."""

    @outer.group("middle")
    def middle():
        """Middle."""

    @middle.command("leaf")
    @click.argument("first")
    @click.argument("rest", nargs=-1)
    def leaf(first, rest):
        """Deeply nested leaf."""

    return root


def test_synthetic_type_mapping(synthetic_app):
    registry = CommandRegistry.from_click_app(synthetic_app)
    options = {o.name: o for o in registry.get("types").options}
    assert options["text"].type is ArgumentType.STRING
    assert options["count"].type is ArgumentType.INT
    assert options["ratio"].type is ArgumentType.FLOAT
    assert options["toggle"].type is ArgumentType.BOOL
    assert options["boolish"].type is ArgumentType.BOOL
    assert options["where"].type is ArgumentType.PATH
    assert options["mode"].type is ArgumentType.CHOICE
    assert options["mode"].choices == ("a", "b")


def test_int_range_degrades_to_string_with_raw_type_preserved(synthetic_app):
    registry = CommandRegistry.from_click_app(synthetic_app)
    ranged = next(o for o in registry.get("types").options if o.name == "ranged")
    assert ranged.type is ArgumentType.STRING
    assert ranged.raw_click_type == "IntRange"


def test_custom_param_type_degrades_to_string_with_raw_type_preserved(synthetic_app):
    registry = CommandRegistry.from_click_app(synthetic_app)
    weird = next(o for o in registry.get("types").options if o.name == "weird")
    assert weird.type is ArgumentType.STRING
    assert weird.raw_click_type == "_CustomType"


def test_recursion_is_structural_to_arbitrary_depth(synthetic_app):
    """Groups are found via isinstance(MultiCommand), not a hardcoded name list."""
    registry = CommandRegistry.from_click_app(synthetic_app)
    leaf = registry.get("outer.middle.leaf")
    assert leaf is not None
    assert leaf.path == ("outer", "middle", "leaf")
    assert leaf.category == "outer.middle"


def test_variadic_argument_is_marked_multiple(synthetic_app):
    registry = CommandRegistry.from_click_app(synthetic_app)
    leaf = registry.get("outer.middle.leaf")
    by_name = {a.name: a for a in leaf.arguments}
    assert by_name["first"].multiple is False
    assert by_name["rest"].multiple is True
    # declared order preserved for positional serialization
    assert [a.name for a in leaf.arguments] == ["first", "rest"]


def test_help_option_is_not_reflected_as_an_option(synthetic_app):
    registry = CommandRegistry.from_click_app(synthetic_app)
    assert "help" not in {o.name for o in registry.get("types").options}


# ---------------------------------------------------------------------------
# Python-keyword / underscore-vs-dash param name collisions
# ---------------------------------------------------------------------------

@pytest.fixture
def keyword_collision_app():
    @click.group()
    def root():
        """Root."""

    @root.command("kw")
    @click.option("--class", "class_", default=None)  # "class" is a Python keyword
    @click.option("--in", "in_", default=None)  # "in" is a Python keyword
    @click.option("--long-option-name", "long_option_name", default=None)
    def kw(**kwargs):
        """Command with keyword-colliding param names."""

    return root


def test_option_name_that_collides_with_python_keyword_is_reflected_safely(keyword_collision_app):
    registry = CommandRegistry.from_click_app(keyword_collision_app)
    options = {o.name: o for o in registry.get("kw").options}
    assert options["class_"].cli_flag == "--class"
    assert options["in_"].cli_flag == "--in"


def test_option_name_with_underscores_maps_to_dash_cli_flag(keyword_collision_app):
    registry = CommandRegistry.from_click_app(keyword_collision_app)
    long_opt = next(o for o in registry.get("kw").options if o.name == "long_option_name")
    assert long_opt.cli_flag == "--long-option-name"


# ---------------------------------------------------------------------------
# multiple-option default and empty-choice/empty-default boundaries
# ---------------------------------------------------------------------------

def test_multiple_option_default_empty_tuple_does_not_raise(synthetic_app):
    @synthetic_app.command("multi-default")
    @click.option("--tag", "tags", multiple=True)
    def multi_default(**kwargs):
        """Multiple option with no default values supplied."""

    registry = CommandRegistry.from_click_app(synthetic_app)
    tags = next(o for o in registry.get("multi-default").options if o.name == "tags")
    assert tags.multiple is True
    # Click itself defaults an unspecified `multiple=True` option to None
    # (not an empty tuple) -- confirm reflection passes that through as-is
    # rather than raising or fabricating a different sentinel.
    assert tags.default is None


# ---------------------------------------------------------------------------
# SD-CONSOLE-010 — no cache, fresh reflection every call
# ---------------------------------------------------------------------------

def test_from_click_app_rebuilds_on_every_call(synthetic_app):
    first = CommandRegistry.from_click_app(synthetic_app)
    second = CommandRegistry.from_click_app(synthetic_app)
    assert first is not second
    assert first.get("types") is not second.get("types")
    assert first.get("types") == second.get("types")


def test_registry_reflects_newly_added_command_without_restart(synthetic_app):
    assert CommandRegistry.from_click_app(synthetic_app).get("added-later") is None

    @synthetic_app.command("added-later")
    def added_later():
        """Added after the first reflection."""

    assert CommandRegistry.from_click_app(synthetic_app).get("added-later") is not None


# ---------------------------------------------------------------------------
# Container protocol
# ---------------------------------------------------------------------------

def test_registry_container_protocol(synthetic_app):
    registry = CommandRegistry.from_click_app(synthetic_app)
    assert len(registry) == len(registry.ids())
    assert "types" in registry
    assert all(isinstance(d, CommandDescriptor) for d in registry)
    assert registry.commands == tuple(registry)


def test_registry_is_not_built_from_private_dia_cli_internals():
    """SD-CONSOLE-001: reflection uses the public MultiCommand API only."""
    import inspect

    from dia_console import registry as registry_module

    source = inspect.getsource(registry_module)
    assert "_find_modules" not in source
    assert "cli_main" not in source
