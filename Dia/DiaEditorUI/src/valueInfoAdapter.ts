// Adapters that turn schema sources into ValueInfoLike for ValueLabel / ValueTooltip
// (DiaValueSchema Feature 7). Both return null when there is no description, so callers
// render the plain label for undescribed fields. The result pairs the entry with the
// `forId` (component type id / sidecar id) that ValueLabel needs for "Defined in".
import type { ValueInfoBinding, ValueInfoLike } from './valueInfo';

/** A component-field entry from a `.diaschema` file. */
export interface ComponentSchemaField {
  name: string;
  kind: string;
  label?: string;
  description?: string;
  unit?: string;
  hint?: string;
  min?: number;
  max?: number;
  default?: number | string | boolean;
}

export function componentFieldToValueInfo(
  field: ComponentSchemaField,
  componentTypeId: string,
): ValueInfoBinding | null {
  if (!field.description) return null;
  const entry: ValueInfoLike = {
    key: field.name,
    kind: field.kind,
    label: field.label ?? field.name,
    description: field.description,
  };
  if (field.unit !== undefined) entry.unit = field.unit;
  if (field.hint !== undefined) entry.hint = field.hint;
  if (field.min !== undefined) entry.min = field.min;
  if (field.max !== undefined) entry.max = field.max;
  if (field.default !== undefined) entry.default = field.default;
  return { entry, forId: componentTypeId };
}

/** A service entry from `valueschema.get` (already ValueInfo-shaped; label defaults to key). */
export function serviceEntryToValueInfo(
  raw: Partial<ValueInfoLike> & { key: string },
  forId: string,
): ValueInfoBinding | null {
  if (!raw.description) return null;
  return { entry: { ...raw, label: raw.label ?? raw.key, description: raw.description }, forId };
}
