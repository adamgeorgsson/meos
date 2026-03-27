import { forwardRef } from "react";

export type FormInputProps = React.InputHTMLAttributes<HTMLInputElement>;

export const FormInput = forwardRef<HTMLInputElement, FormInputProps>(
  function FormInput(props, ref) {
    return (
      <input
        ref={ref}
        className="w-full border rounded px-3 py-2 text-sm focus:outline-none focus:ring-2 focus:ring-blue-500"
        {...props}
      />
    );
  }
);
