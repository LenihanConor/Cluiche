// ValueInfo-shaped data accepted by ValueLabel / ValueTooltip. Field names follow the
// DiaValueSchema sidecar format. Only key/label/description are required so that a
// `.diaschema` component field or a `valueschema.get` entry can both be passed.

export interface ValueInfoEnumValue {
  id: string;
  label: string;
  description: string;
  /** Optional "#rrggbb" swatch colour (enum palettes). */
  colour?: string;
}

export interface ValueInfoUi {
  group?: string;
  step?: number;
  decimals?: number;
  ramp?: string;
  delta_ramp?: string;
  layer?: string;
  hidden?: boolean;
}

export interface ValueInfoLike {
  key: string;
  label: string;
  description: string;
  kind?: string;
  hint?: string;
  unit?: string;
  type?: string;
  min?: number;
  max?: number;
  default?: number | string | boolean;
  affects?: string[];
  related?: string[];
  source?: string;
  values?: ValueInfoEnumValue[];
  ui?: ValueInfoUi;
}

/** Entry plus the id of the sidecar/component it is defined in (the `for` of the fixture). */
export interface ValueInfoBinding {
  entry: ValueInfoLike;
  forId: string;
}

/** Keys that have a matching control on the panel right now. */
export type ResolvableKeys = ReadonlyArray<string> | ReadonlySet<string>;
