import { describe, it, expect } from 'vitest';
import { componentFieldToValueInfo, serviceEntryToValueInfo } from './valueInfoAdapter';

describe('componentFieldToValueInfo', () => {
  it('maps a described field', () => {
    const r = componentFieldToValueInfo(
      { name: 'speed', kind: 'float', label: 'Speed', description: 'How fast.', unit: 'm/s', hint: 'h', min: 0, max: 9, default: 0 },
      'Mover',
    );
    expect(r).toEqual({
      forId: 'Mover',
      entry: { key: 'speed', kind: 'float', label: 'Speed', description: 'How fast.', unit: 'm/s', hint: 'h', min: 0, max: 9, default: 0 },
    });
  });

  it('defaults label to name and omits absent fields', () => {
    const r = componentFieldToValueInfo({ name: 'hp', kind: 'int', description: 'Health.' }, 'Body');
    expect(r?.entry).toEqual({ key: 'hp', kind: 'int', label: 'hp', description: 'Health.' });
  });

  it('returns null without a description', () => {
    expect(componentFieldToValueInfo({ name: 'hp', kind: 'int', label: 'HP' }, 'Body')).toBeNull();
    expect(componentFieldToValueInfo({ name: 'hp', kind: 'int', description: '' }, 'Body')).toBeNull();
  });
});

describe('serviceEntryToValueInfo', () => {
  it('passes through service fields and defaults label to key', () => {
    const r = serviceEntryToValueInfo({ key: 'a.b', description: 'D', affects: ['x'] }, 'cow.params');
    expect(r).toEqual({ forId: 'cow.params', entry: { key: 'a.b', label: 'a.b', description: 'D', affects: ['x'] } });
  });

  it('returns null without a description', () => {
    expect(serviceEntryToValueInfo({ key: 'a' }, 'f')).toBeNull();
  });
});
